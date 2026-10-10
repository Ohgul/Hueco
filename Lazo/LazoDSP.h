#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

// ===========================================================================
// Lazo: compresor de bus estéreo al estilo de los compresores de consola
// clásicos (VCA, feed-forward, detección enlazada en estéreo).
// ===========================================================================

namespace LazoID
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ratio     = "ratio";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* release   = "release";
    inline constexpr const char* makeup    = "makeup";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* scHpf     = "scHpf";
    inline constexpr const char* range     = "range";
    inline constexpr const char* clip      = "clip";
    inline constexpr const char* extSc     = "extSc";
}

namespace LazoTables
{
    inline const juce::StringArray attackNames  { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" };
    inline const float attackMs[]               { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
    inline const juce::StringArray releaseNames { "0.1 s", "0.2 s", "0.4 s", "0.6 s", "0.8 s", "1.2 s", "Auto" };
    inline const float releaseS[]               { 0.1f, 0.2f, 0.4f, 0.6f, 0.8f, 1.2f, -1.0f };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLazoLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using Attr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

    p.push_back (std::make_unique<F> (ParameterID { LazoID::threshold, 1 }, "Umbral",
                                      NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -10.0f, Attr().withStringFromValueFunction (db)));

    NormalisableRange<float> ratioRange (1.0f, 20.0f, 0.01f);
    ratioRange.setSkewForCentre (4.0f);
    p.push_back (std::make_unique<F> (ParameterID { LazoID::ratio, 1 }, "Ratio", ratioRange, 4.0f,
                                      Attr().withStringFromValueFunction ([] (float v, int)
                                      { return String (v, v < 10.0f ? 1 : 0) + ":1"; })));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { LazoID::attack, 1 }, "Ataque", LazoTables::attackNames, 3));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { LazoID::release, 1 }, "Liberacion", LazoTables::releaseNames, 6));
    p.push_back (std::make_unique<F> (ParameterID { LazoID::makeup, 1 }, "Ganancia",
                                      NormalisableRange<float> (0.0f, 20.0f, 0.1f), 0.0f, Attr().withStringFromValueFunction (db)));
    p.push_back (std::make_unique<F> (ParameterID { LazoID::mix, 1 }, "Mezcla",
                                      NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f, Attr().withStringFromValueFunction (pct)));

    NormalisableRange<float> hpf (0.0f, 2000.0f, 1.0f);
    hpf.setSkewForCentre (150.0f);
    p.push_back (std::make_unique<F> (ParameterID { LazoID::scHpf, 1 }, "Filtro SC", hpf, 0.0f,
                                      Attr().withStringFromValueFunction ([] (float v, int)
                                      { return v < 20.0f ? String ("Apagado") : String (roundToInt (v)) + " Hz"; })));
    p.push_back (std::make_unique<F> (ParameterID { LazoID::range, 1 }, "Rango",
                                      NormalisableRange<float> (1.0f, 40.0f, 0.1f), 40.0f, Attr().withStringFromValueFunction (db)));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { LazoID::clip, 1 }, "Clip", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { LazoID::extSc, 1 }, "Sidechain externo", false));

    return { p.begin(), p.end() };
}

// Presets (valores reales). Ataque y liberación son índices de las tablas.
struct LazoPreset { const char* name; float thr, ratio, atk, rel, makeup, mix, hpf, range; bool clip; };

inline const std::vector<LazoPreset>& lazoPresets()
{
    static const std::vector<LazoPreset> p {
        { "Inicial",            -10.0f, 4.0f,  3, 6, 0.0f, 1.0f,   0.0f, 40.0f, false },
        { "Pegamento de mezcla", -14.0f, 2.0f,  4, 6, 2.0f, 1.0f,  60.0f, 40.0f, false },
        { "Master suave",        -8.0f, 1.5f,  5, 6, 1.0f, 1.0f,  90.0f,  4.0f, true  },
        { "Bus de bateria",     -20.0f, 4.0f,  4, 6, 5.0f, 0.8f,   0.0f, 40.0f, true  },
        { "Bombeo",             -24.0f, 10.0f, 3, 1, 8.0f, 1.0f,   0.0f, 40.0f, false },
        { "Aplastar (paralelo)",-32.0f, 20.0f, 0, 0, 14.0f, 0.4f,  0.0f, 40.0f, true  },
        { "Voz",                -18.0f, 3.0f,  2, 2, 4.0f, 1.0f, 120.0f, 12.0f, false },
    };
    return p;
}

// ---------------------------------------------------------------------------
// Núcleo del compresor
class LazoCompressor
{
public:
    struct Settings
    {
        float thresholdDb = -10.0f, ratio = 4.0f, attackMs = 3.0f, releaseS = -1.0f; // -1 = auto
        float makeupDb = 0.0f, mix = 1.0f, scHpfHz = 0.0f, rangeDb = 40.0f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        grFast = grSlow = 0.0f;
        for (auto& f : hpfState) f = {};
        makeupSmoothed.reset (sr, 0.05);
        mixSmoothed.reset (sr, 0.05);
        makeupSmoothed.setCurrentAndTargetValue (1.0f);
        mixSmoothed.setCurrentAndTargetValue (1.0f);
        lastHpf = -1.0f;
    }

    // Procesa en sitio. scL/scR: cadena lateral (si es nullptr se usa la propia señal).
    // Devuelve la reducción de ganancia máxima del bloque (dB, positiva).
    float process (float* left, float* right, const float* scL, const float* scR, int n, const Settings& s)
    {
        updateHpf (s.scHpfHz);
        makeupSmoothed.setTargetValue (juce::Decibels::decibelsToGain (s.makeupDb));
        mixSmoothed.setTargetValue (s.mix);

        const float atk = coeff (s.attackMs * 0.001f);
        const bool autoRel = s.releaseS < 0.0f;
        const float rel = coeff (autoRel ? 0.1f : s.releaseS);
        // Liberación automática: una parte rápida (transitorios) que regresa hacia
        // una parte lenta (nivel general de la mezcla). Así "respira" con la música.
        const float slowAtk = coeff (0.25f);
        const float slowRel = coeff (0.7f);
        const float slope = 1.0f - 1.0f / juce::jmax (1.0f, s.ratio);
        constexpr float knee = 6.0f;
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float l = left[i], r = right[i];

            // Cadena lateral: filtro pasa-altos opcional, detección enlazada (máximo de L/R)
            float sl = scL != nullptr ? scL[i] : l;
            float srr = scR != nullptr ? scR[i] : r;
            if (hpfOn) { sl = hpf (0, sl); srr = hpf (1, srr); }
            const float peak = juce::jmax (std::abs (sl), std::abs (srr));
            const float levelDb = peak > 1.0e-6f ? 20.0f * std::log10 (peak) : -120.0f;

            // Curva estática con rodilla suave
            const float over = levelDb - s.thresholdDb;
            float target = 0.0f;
            if (over >= knee * 0.5f)       target = over * slope;
            else if (over > -knee * 0.5f)  target = slope * juce::square (over + knee * 0.5f) / (2.0f * knee);
            target = juce::jmin (target, s.rangeDb);

            // Balística en el dominio de la reducción de ganancia
            if (autoRel)
            {
                grSlow += (target - grSlow) * (target > grSlow ? slowAtk : slowRel);
                if (target > grFast) grFast += (target - grFast) * atk;
                else                 grFast += (juce::jmax (target, grSlow) - grFast) * rel;
            }
            else
            {
                grFast += (target - grFast) * (target > grFast ? atk : rel);
            }

            const float gain = juce::Decibels::decibelsToGain (-grFast) * makeupSmoothed.getNextValue();
            const float m = mixSmoothed.getNextValue();
            left[i]  = l * (m * gain + (1.0f - m));
            right[i] = r * (m * gain + (1.0f - m));
            maxGr = juce::jmax (maxGr, grFast);
        }
        return maxGr;
    }

    // Clip suave: lineal hasta -2.2 dBFS, luego satura de forma redonda hasta -0.3 dBFS
    // (el margen evita que el filtro de sobremuestreo pase de 0 dBFS)
    static float softClip (float x)
    {
        constexpr float t = 0.78f, ceiling = 0.966f;
        const float a = std::abs (x);
        if (a <= t) return x;
        const float y = t + (ceiling - t) * std::tanh ((a - t) / (ceiling - t));
        return std::copysign (y, x);
    }

private:
    float coeff (float seconds) const
    {
        return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-5f, seconds) * (float) sr));
    }

    struct Biquad { float z1 = 0.0f, z2 = 0.0f; };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<Biquad, 2> hpfState;
    bool hpfOn = false;
    float lastHpf = -1.0f;

    void updateHpf (float hz)
    {
        if (hz == lastHpf) return;
        lastHpf = hz;
        hpfOn = hz >= 20.0f;
        if (! hpfOn) return;
        // Pasa-altos de 2º orden (Butterworth)
        const double w = juce::MathConstants<double>::twoPi * hz / sr;
        const double c = std::cos (w), sn = std::sin (w), alpha = sn / (2.0 * 0.7071);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 + c) / 2.0 / a0);
        b1 = (float) (-(1.0 + c) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    float hpf (int ch, float x)
    {
        auto& st = hpfState[(size_t) ch];
        const float y = b0 * x + st.z1;
        st.z1 = b1 * x - a1 * y + st.z2;
        st.z2 = b2 * x - a2 * y;
        return y;
    }

    double sr = 44100.0;
    float grFast = 0.0f, grSlow = 0.0f;
    juce::SmoothedValue<float> makeupSmoothed, mixSmoothed;
};
