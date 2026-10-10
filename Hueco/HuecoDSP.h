#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ===========================================================================
// Hueco: abre espacio en una pista bajando solo las frecuencias que ocupa
// otra pista (la que llega por sidechain). 32 bandas, latencia cero.
// ===========================================================================

namespace HuecoID
{
    inline constexpr const char* amount   = "amount";
    inline constexpr const char* lowFreq  = "lowFreq";
    inline constexpr const char* highFreq = "highFreq";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* release  = "release";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* mode     = "mode";
}

inline const juce::StringArray& huecoModeNames()
{
    static const juce::StringArray names { "Estereo", "Izq / Der", "Medio / Lados" };
    return names;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createHuecoLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using Attr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto hz  = [] (float v, int) { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz"; };
    auto ms  = [] (float v, int) { return String (roundToInt (v)) + " ms"; };

    p.push_back (std::make_unique<F> (ParameterID { HuecoID::amount, 1 }, "Cantidad",
                                      NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, Attr().withStringFromValueFunction (pct)));

    NormalisableRange<float> lowR (20.0f, 2000.0f, 1.0f);   lowR.setSkewForCentre (150.0f);
    NormalisableRange<float> highR (500.0f, 20000.0f, 1.0f); highR.setSkewForCentre (4000.0f);
    p.push_back (std::make_unique<F> (ParameterID { HuecoID::lowFreq, 1 },  "Desde",  lowR, 20.0f, Attr().withStringFromValueFunction (hz)));
    p.push_back (std::make_unique<F> (ParameterID { HuecoID::highFreq, 1 }, "Hasta", highR, 20000.0f, Attr().withStringFromValueFunction (hz)));

    NormalisableRange<float> atkR (1.0f, 200.0f, 0.1f); atkR.setSkewForCentre (15.0f);
    NormalisableRange<float> relR (10.0f, 1000.0f, 1.0f); relR.setSkewForCentre (150.0f);
    p.push_back (std::make_unique<F> (ParameterID { HuecoID::attack, 1 },  "Ataque",     atkR, 10.0f,  Attr().withStringFromValueFunction (ms)));
    p.push_back (std::make_unique<F> (ParameterID { HuecoID::release, 1 }, "Liberacion", relR, 150.0f, Attr().withStringFromValueFunction (ms)));
    p.push_back (std::make_unique<F> (ParameterID { HuecoID::mix, 1 }, "Mezcla",
                                      NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f, Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { HuecoID::mode, 1 }, "Modo", huecoModeNames(), 0));

    return { p.begin(), p.end() };
}

class HuecoEngine
{
public:
    static constexpr int numBands = 32;
    static constexpr float minHz = 40.0f, maxHz = 16000.0f;
    static constexpr float maxCutDb = 30.0f;
    static constexpr int updateEvery = 32;

    struct Settings
    {
        float amount = 0.5f, lowHz = 20.0f, highHz = 20000.0f, attackMs = 10.0f, releaseMs = 150.0f, mix = 1.0f;
        int mode = 0;   // 0 estéreo, 1 izq/der, 2 medio/lados
    };

    static float bandFreq (int k) { return minHz * std::pow (maxHz / minHz, (float) k / (float) (numBands - 1)); }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double ratio = std::pow ((double) maxHz / minHz, 1.0 / (numBands - 1));
        q = std::sqrt (ratio) / (ratio - 1.0);  // ancho de banda = separación entre bandas
        for (int k = 0; k < numBands; ++k)
        {
            const double w0 = juce::MathConstants<double>::twoPi * juce::jmin ((double) bandFreq (k), sr * 0.45) / sr;
            cosW[(size_t) k] = std::cos (w0);
            alpha[(size_t) k] = std::sin (w0) / (2.0 * q);

            // Pasa-banda de análisis (ganancia 0 dB en el centro)
            const double a0 = 1.0 + alpha[(size_t) k];
            auto& bp = bpCoef[(size_t) k];
            bp.b0 = alpha[(size_t) k] / a0; bp.b1 = 0.0; bp.b2 = -alpha[(size_t) k] / a0;
            bp.a1 = -2.0 * cosW[(size_t) k] / a0; bp.a2 = (1.0 - alpha[(size_t) k]) / a0;
        }
        reset();
    }

    void reset()
    {
        for (auto& ch : analysis) for (auto& s : ch) s = {};
        for (auto& ch : analysis2) for (auto& s : ch) s = {};
        for (auto& ch : eqState)  for (auto& s : ch) s = {};
        for (auto& ch : env) ch.fill (0.0f);
        for (auto& ch : cutDb) ch.fill (0.0f);
        for (auto& ch : appliedDb) ch.fill (0.0f);
        for (int c = 0; c < 2; ++c) for (int k = 0; k < numBands; ++k) setEqGain (c, k, 0.0f);
        counter = 0;
        for (int k = 0; k < numBands; ++k) { displayLevel[(size_t) k] = -100.0f; displayCut[(size_t) k] = 0.0f; }
        mixSm.reset (sr, 0.05);
        mixSm.setCurrentAndTargetValue (1.0f);
    }

    // main: 2 canales (L,R) en sitio. sc: cadena lateral (L,R) o nullptr si no hay.
    void process (float* L, float* R, const float* scL, const float* scR, int n, const Settings& s)
    {
        // Detector rápido y fijo; el ataque/liberación del usuario se aplica al corte (en dB)
        const float detAtk = coeff (0.002f), detRel = coeff (0.03f);
        const float atk = blockCoeff (s.attackMs * 0.001f), rel = blockCoeff (s.releaseMs * 0.001f);
        mixSm.setTargetValue (s.mix);
        const bool haveSc = scL != nullptr;
        const int analysisChannels = (s.mode == 0) ? 1 : 2;

        for (int i = 0; i < n; ++i)
        {
            // --- Cadena lateral → niveles por banda ---
            if (haveSc)
            {
                float a[2];
                const float sl = scL[i], srr = scR != nullptr ? scR[i] : scL[i];
                if (s.mode == 0)      { a[0] = 0.5f * (sl + srr); a[1] = a[0]; }
                else if (s.mode == 1) { a[0] = sl; a[1] = srr; }
                else                  { a[0] = 0.5f * (sl + srr); a[1] = 0.5f * (sl - srr); }

                for (int c = 0; c < analysisChannels; ++c)
                    for (int k = 0; k < numBands; ++k)
                    {
                        const double y1 = biquad (bpCoef[(size_t) k], analysis[(size_t) c][(size_t) k], a[c]);
                        const float y = std::abs ((float) biquad (bpCoef[(size_t) k], analysis2[(size_t) c][(size_t) k], y1));
                        float& e = env[(size_t) c][(size_t) k];
                        e += (y - e) * (y > e ? detAtk : detRel);
                    }
            }

            // --- Cada 32 muestras: recalcular cuánto cortar en cada banda ---
            if (counter++ % updateEvery == 0)
                updateGains (s, haveSc, analysisChannels, atk, rel);

            // --- Señal principal → ecualizador dinámico ---
            const float l = L[i], r = R[i];
            double x0, x1;
            if (s.mode == 2) { x0 = 0.5 * (l + r); x1 = 0.5 * (l - r); }
            else             { x0 = l; x1 = r; }

            for (int k = 0; k < numBands; ++k)
            {
                x0 = biquad (eqCoef[0][(size_t) k], eqState[0][(size_t) k], x0);
                x1 = biquad (eqCoef[1][(size_t) k], eqState[1][(size_t) k], x1);
            }

            float wl, wr;
            if (s.mode == 2) { wl = (float) (x0 + x1); wr = (float) (x0 - x1); }
            else             { wl = (float) x0; wr = (float) x1; }

            const float m = mixSm.getNextValue();
            L[i] = wl * m + l * (1.0f - m);
            R[i] = wr * m + r * (1.0f - m);
        }
    }

    // Para la pantalla: nivel del sidechain y corte aplicado por banda (canal 0)
    std::array<std::atomic<float>, numBands> displayLevel, displayCut;

private:
    struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State { double z1 = 0, z2 = 0; };

    static double biquad (const Coef& c, State& s, double x)
    {
        const double y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    float coeff (float seconds) const { return 1.0f - std::exp (-1.0f / (juce::jmax (1.0e-4f, seconds) * (float) sr)); }
    float blockCoeff (float seconds) const { return 1.0f - std::exp (-(float) updateEvery / (juce::jmax (1.0e-4f, seconds) * (float) sr)); }

    // Ecualizador "peaking" con ganancia negativa (fórmulas RBJ)
    void setEqGain (int c, int k, float gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double al = alpha[(size_t) k], cw = cosW[(size_t) k];
        const double a0 = 1.0 + al / A;
        auto& e = eqCoef[(size_t) c][(size_t) k];
        e.b0 = (1.0 + al * A) / a0;
        e.b1 = -2.0 * cw / a0;
        e.b2 = (1.0 - al * A) / a0;
        e.a1 = -2.0 * cw / a0;
        e.a2 = (1.0 - al / A) / a0;
    }

    static float rangeWeight (float f, float lowHz, float highHz)
    {
        // 1 dentro del rango, cae suavemente (media octava) fuera de él
        float w = 1.0f;
        if (f < lowHz)  w = juce::jmax (0.0f, 1.0f - std::log2 (lowHz / f) * 2.0f);
        if (f > highHz) w = juce::jmin (w, juce::jmax (0.0f, 1.0f - std::log2 (f / highHz) * 2.0f));
        return w;
    }

    void updateGains (const Settings& s, bool haveSc, int analysisChannels, float atk, float rel)
    {
        for (int c = 0; c < 2; ++c)
        {
            const int src = analysisChannels == 1 ? 0 : c;

            // Nivel de la banda más fuerte del sidechain (para cortar solo donde de verdad hay energía)
            float loudest = -100.0f;
            std::array<float, numBands> levelDb;
            for (int k = 0; k < numBands; ++k)
            {
                levelDb[(size_t) k] = haveSc ? juce::Decibels::gainToDecibels (env[(size_t) src][(size_t) k], -100.0f) : -100.0f;
                loudest = juce::jmax (loudest, levelDb[(size_t) k]);
            }

            for (int k = 0; k < numBands; ++k)
            {
                float target = 0.0f;
                if (haveSc)
                {
                    // Más fuerte el sidechain en esa banda → más corte (hasta 30 dB)
                    const float absolute = juce::jlimit (0.0f, maxCutDb, (levelDb[(size_t) k] + 45.0f) * 0.65f);
                    const float relative = juce::jlimit (0.0f, 1.0f, (levelDb[(size_t) k] - loudest + 30.0f) / 20.0f);
                    target = absolute * relative * s.amount * rangeWeight (bandFreq (k), s.lowHz, s.highHz);
                }

                float& cut = cutDb[(size_t) c][(size_t) k];
                cut += (target - cut) * (target > cut ? atk : rel);
                if (std::abs (cut - target) < 0.02f) cut = target;

                float& applied = appliedDb[(size_t) c][(size_t) k];
                if (std::abs (cut - applied) > 0.01f || (cut == target && applied != target))
                {
                    applied = cut;
                    setEqGain (c, k, -cut);
                }
                if (c == 0)
                {
                    displayLevel[(size_t) k] = levelDb[(size_t) k];
                    displayCut[(size_t) k] = cut;
                }
            }
        }
    }

    double sr = 44100.0, q = 4.0;
    std::array<double, numBands> cosW {}, alpha {};
    std::array<Coef, numBands> bpCoef;
    std::array<std::array<Coef, numBands>, 2> eqCoef;
    std::array<std::array<State, numBands>, 2> analysis, analysis2, eqState;
    std::array<std::array<float, numBands>, 2> env {}, cutDb {}, appliedDb {};
    juce::SmoothedValue<float> mixSm;
    unsigned int counter = 0;
};
