// Pruebas de Lazo y Hueco sin DAW.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Lazo/LazoProcessor.h"
#include "../Hueco/HuecoProcessor.h"

static constexpr double sr = 48000.0;
static constexpr int block = 256;
static int failures = 0;

static void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FALLO", what.toRawUTF8());
    if (! ok) ++failures;
}

static void setParam (juce::AudioProcessorValueTreeState& s, const char* id, float v)
{
    auto* p = s.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}

// Configura 2 entradas (principal + sidechain) estéreo
static void setupBuses (juce::AudioProcessor& p)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    const bool ok = p.setBusesLayout (layout);
    if (! ok) std::printf ("  (no se pudo configurar el sidechain)\n");
    p.prepareToPlay (sr, block);
}

// Procesa: main (2 canales) y sc (2 canales o vacío). Devuelve la salida principal.
static juce::AudioBuffer<float> run (juce::AudioProcessor& p, const juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sc)
{
    p.prepareToPlay (sr, block);   // empieza cada prueba desde cero
    const int n = main.getNumSamples();
    juce::AudioBuffer<float> out (2, n);
    juce::AudioBuffer<float> buf (4, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += block)
    {
        const int m = juce::jmin (block, n - pos);
        buf.setSize (4, m, false, false, true);
        buf.clear();
        for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, main, ch, pos, m);
        if (sc != nullptr) for (int ch = 0; ch < 2; ++ch) buf.copyFrom (2 + ch, 0, *sc, ch, pos, m);
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, m);
    }
    return out;
}

static juce::AudioBuffer<float> sine (std::initializer_list<std::pair<double, float>> parts, double seconds, bool antiphase = false)
{
    juce::AudioBuffer<float> b (2, (int) (sr * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        double v = 0.0;
        for (auto& [hz, amp] : parts) v += amp * std::sin (juce::MathConstants<double>::twoPi * hz * i / sr);
        b.setSample (0, i, (float) v);
        b.setSample (1, i, (float) (antiphase ? -v : v));
    }
    return b;
}

static float peakDb (const juce::AudioBuffer<float>& b, int start, int len)
{
    return juce::Decibels::gainToDecibels (b.getMagnitude (start, len), -120.0f);
}

// Nivel de una frecuencia (Goertzel) en dB
static float toneDb (const juce::AudioBuffer<float>& b, int ch, double hz, int start, int len)
{
    const double w = juce::MathConstants<double>::twoPi * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < len; ++i)
    {
        const double s0 = b.getSample (ch, start + i) + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - c * s1 * s2;
    return (float) juce::Decibels::gainToDecibels (std::sqrt (power) * 2.0 / len, -120.0);
}

static void snapshot (juce::AudioProcessor& p, const juce::File& f)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    // Deja correr los temporizadores un poco (sin bucle de mensajes llamamos al timer a mano)
    if (auto* t = dynamic_cast<juce::Timer*> (ed.get()))
        for (int i = 0; i < 4; ++i) t->timerCallback();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    f.deleteFile();
    juce::FileOutputStream fos (f);
    juce::PNGImageFormat().writeImageToStream (img, fos);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    // ======================= LAZO =======================
    std::printf ("LAZO\n");
    {
        LazoProcessor p;
        setupBuses (p);
        auto& s = p.state;
        const int lat = p.getLatencySamples();
        std::printf ("  latencia: %d muestras\n", lat);

        // 1) Curva estática: umbral -20, ratio 4, seno a -6 dBFS → reducción 10.5 dB → salida ≈ -16.5
        setParam (s, LazoID::threshold, -20.0f); setParam (s, LazoID::ratio, 4.0f);
        setParam (s, LazoID::attack, 0); setParam (s, LazoID::release, 5);
        setParam (s, LazoID::makeup, 0.0f); setParam (s, LazoID::mix, 1.0f); setParam (s, LazoID::clip, 0.0f);
        auto in = sine ({ { 1000.0, 0.5f } }, 1.0);
        auto out = run (p, in, nullptr);
        const float outPk = peakDb (out, (int) (sr * 0.7), (int) (sr * 0.2));
        check (std::abs (outPk - (-16.5f)) < 1.0f, "ratio 4:1, umbral -20: salida " + juce::String (outPk, 1) + " dB (esperado -16.5)");

        // 2) Ratio continuo: 2:1 → reducción 7 dB → -13
        setParam (s, LazoID::ratio, 2.0f);
        out = run (p, in, nullptr);
        const float out2 = peakDb (out, (int) (sr * 0.7), (int) (sr * 0.2));
        check (std::abs (out2 - (-13.0f)) < 1.0f, "ratio 2:1: salida " + juce::String (out2, 1) + " dB (esperado -13)");

        // 3) Rango: limita la reducción a 3 dB → -9
        setParam (s, LazoID::range, 3.0f);
        out = run (p, in, nullptr);
        const float out3 = peakDb (out, (int) (sr * 0.7), (int) (sr * 0.2));
        check (std::abs (out3 - (-9.0f)) < 1.0f, "rango 3 dB: salida " + juce::String (out3, 1) + " dB (esperado -9)");
        setParam (s, LazoID::range, 40.0f);

        // 4) Mezcla 0 = transparente (comparando con la entrada retrasada por la latencia)
        setParam (s, LazoID::mix, 0.0f);
        out = run (p, in, nullptr);
        float maxErr = 0.0f;
        for (int i = (int) (sr * 0.2); i < (int) (sr * 0.9); ++i)
            maxErr = juce::jmax (maxErr, std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
        check (maxErr < 0.01f, "mezcla 0 %: transparente (error max " + juce::String (maxErr, 4) + ")");
        setParam (s, LazoID::mix, 1.0f);

        // 5) Clip: entrada fuerte con ganancia → nunca pasa de 0 dBFS
        setParam (s, LazoID::clip, 1.0f); setParam (s, LazoID::makeup, 20.0f); setParam (s, LazoID::threshold, 0.0f);
        auto loud = sine ({ { 200.0, 0.9f }, { 3000.0, 0.3f } }, 0.5);
        out = run (p, loud, nullptr);
        const float clipPk = out.getMagnitude (0, out.getNumSamples());
        check (clipPk <= 1.001f, "clip: pico de salida " + juce::String (clipPk, 3) + " (<= 1.0)");
        setParam (s, LazoID::clip, 0.0f); setParam (s, LazoID::makeup, 0.0f);

        // 6) Liberación automática: tras un golpe fuerte vuelve a 0 de reducción
        setParam (s, LazoID::threshold, -20.0f); setParam (s, LazoID::ratio, 4.0f); setParam (s, LazoID::release, 6);
        juce::AudioBuffer<float> burst (2, (int) (sr * 3.0));
        burst.clear();
        for (int i = 0; i < (int) (sr * 0.5); ++i)
            for (int ch = 0; ch < 2; ++ch) burst.setSample (ch, i, 0.7f * (float) std::sin (juce::MathConstants<double>::twoPi * 100.0 * i / sr));
        for (int i = (int) (sr * 0.5); i < burst.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch) burst.setSample (ch, i, 0.02f * (float) std::sin (juce::MathConstants<double>::twoPi * 100.0 * i / sr));
        out = run (p, burst, nullptr);
        const float quietAfter = peakDb (out, (int) (sr * 2.6), (int) (sr * 0.3));
        check (std::abs (quietAfter - juce::Decibels::gainToDecibels (0.02f)) < 1.0f, "auto: se recupera tras el golpe (" + juce::String (quietAfter, 1) + " dB)");
        const float rightAfter = peakDb (out, (int) (sr * 0.55), (int) (sr * 0.05));
        check (rightAfter < juce::Decibels::gainToDecibels (0.02f) - 1.0f, "auto: sigue comprimiendo justo despues del golpe (" + juce::String (rightAfter, 1) + " dB)");

        // 7) Sidechain externo: la pista principal baja cuando suena el sidechain
        auto quiet = sine ({ { 500.0, 0.1f } }, 1.0);
        juce::AudioBuffer<float> scBuf (2, quiet.getNumSamples());
        scBuf.clear();
        for (int i = (int) (sr * 0.5); i < scBuf.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch) scBuf.setSample (ch, i, 0.9f * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * i / sr));
        setParam (s, LazoID::extSc, 1.0f);
        out = run (p, quiet, &scBuf);
        const float before = peakDb (out, (int) (sr * 0.3), (int) (sr * 0.15));
        const float during = peakDb (out, (int) (sr * 0.8), (int) (sr * 0.15));
        check (before - during > 8.0f, "sidechain externo: baja " + juce::String (before - during, 1) + " dB cuando suena el sidechain");
        setParam (s, LazoID::extSc, 0.0f);
        out = run (p, quiet, &scBuf);
        const float during2 = peakDb (out, (int) (sr * 0.8), (int) (sr * 0.15));
        const float before2 = peakDb (out, (int) (sr * 0.3), (int) (sr * 0.15));
        check (std::abs (before2 - during2) < 0.3f, "sidechain apagado: no reacciona al sidechain (" + juce::String (before2, 2) + " vs " + juce::String (during2, 2) + ")");

        // 8) Presets cargan sin problemas
        for (int i = 0; i < (int) lazoPresets().size(); ++i) p.loadPreset (i);
        auto music = sine ({ { 80.0, 0.5f }, { 440.0, 0.2f }, { 2500.0, 0.1f } }, 1.0);
        p.loadPreset (3);
        out = run (p, music, nullptr);
        bool finite = true;
        for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i));
        check (finite, "presets: salida valida");

        // Estado guardado/restaurado
        juce::MemoryBlock mb;
        setParam (s, LazoID::ratio, 7.5f);
        p.getStateInformation (mb);
        setParam (s, LazoID::ratio, 2.0f);
        p.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (s.getRawParameterValue (LazoID::ratio)->load() - 7.5f) < 0.05f, "guardar/abrir proyecto");

        // Captura (con algo de reducción para que se vea la aguja)
        p.loadPreset (3);
        run (p, music, nullptr);
        snapshot (p, outDir.getChildFile ("lazo.png"));
    }

    // ======================= HUECO =======================
    std::printf ("HUECO\n");
    {
        HuecoProcessor p;
        setupBuses (p);
        auto& s = p.state;
        setParam (s, HuecoID::amount, 1.0f);

        auto main = sine ({ { 1000.0, 0.3f }, { 100.0, 0.3f } }, 1.5);
        const int a = (int) (sr * 1.0), len = (int) (sr * 0.4);
        const float ref1k = toneDb (main, 0, 1000.0, a, len), ref100 = toneDb (main, 0, 100.0, a, len);

        // 1) Sin señal de sidechain: transparente
        juce::AudioBuffer<float> silent (2, main.getNumSamples());
        silent.clear();
        auto out = run (p, main, &silent);
        float maxErr = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i) maxErr = juce::jmax (maxErr, std::abs (out.getSample (0, i) - main.getSample (0, i)));
        check (maxErr < 1.0e-3f, "sin sidechain: transparente (error " + juce::String (maxErr, 5) + ")");

        // 2) Sidechain con 1 kHz: baja el 1 kHz de la pista, el 100 Hz casi no se toca
        auto sc1k = sine ({ { 1000.0, 0.5f } }, 1.5);
        out = run (p, main, &sc1k);
        const float d1k = ref1k - toneDb (out, 0, 1000.0, a, len);
        const float d100 = ref100 - toneDb (out, 0, 100.0, a, len);
        check (d1k > 12.0f, "corta 1 kHz: " + juce::String (d1k, 1) + " dB");
        check (d100 < 1.5f, "respeta 100 Hz: " + juce::String (d100, 2) + " dB");
        std::printf ("  (1 kHz -%.1f dB, 100 Hz -%.2f dB)\n", d1k, d100);

        // 3) Cantidad 50 % → corte menor
        setParam (s, HuecoID::amount, 0.5f);
        out = run (p, main, &sc1k);
        const float dHalf = ref1k - toneDb (out, 0, 1000.0, a, len);
        check (dHalf > 3.0f && dHalf < d1k - 3.0f, "cantidad 50 %: " + juce::String (dHalf, 1) + " dB");
        setParam (s, HuecoID::amount, 1.0f);

        // 4) Rango "Hasta 400 Hz": el 1 kHz queda fuera → casi sin corte
        setParam (s, HuecoID::highFreq, 500.0f);
        out = run (p, main, &sc1k);
        const float dOut = ref1k - toneDb (out, 0, 1000.0, a, len);
        check (dOut < 2.0f, "fuera de rango (hasta 500 Hz): " + juce::String (dOut, 2) + " dB");
        setParam (s, HuecoID::highFreq, 20000.0f);

        // 5) Medio/Lados: sidechain al centro → solo se corta el centro
        setParam (s, HuecoID::mode, 2.0f);
        auto sideOnly = sine ({ { 1000.0, 0.3f } }, 1.5, true);   // L = -R (solo lados)
        out = run (p, sideOnly, &sc1k);
        const float dSide = toneDb (sideOnly, 0, 1000.0, a, len) - toneDb (out, 0, 1000.0, a, len);
        check (dSide < 1.0f, "medio/lados: los lados no se tocan (" + juce::String (dSide, 2) + " dB)");
        out = run (p, main, &sc1k);
        const float dMid = ref1k - toneDb (out, 0, 1000.0, a, len);
        check (dMid > 12.0f, "medio/lados: el centro si se corta (" + juce::String (dMid, 1) + " dB)");
        setParam (s, HuecoID::mode, 0.0f);

        // 6) Mezcla 0 → transparente aunque haya sidechain
        setParam (s, HuecoID::mix, 0.0f);
        out = run (p, main, &sc1k);
        maxErr = 0.0f;
        for (int i = (int) (sr * 0.2); i < out.getNumSamples(); ++i) maxErr = juce::jmax (maxErr, std::abs (out.getSample (0, i) - main.getSample (0, i)));
        check (maxErr < 1.0e-3f, "mezcla 0 %: transparente");
        setParam (s, HuecoID::mix, 1.0f);

        // 7) Ataque/liberación: al callar el sidechain, el 1 kHz regresa
        juce::AudioBuffer<float> scStop (sc1k);
        for (int i = (int) (sr * 0.7); i < scStop.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) scStop.setSample (ch, i, 0.0f);
        out = run (p, main, &scStop);
        const float back = ref1k - toneDb (out, 0, 1000.0, (int) (sr * 1.3), (int) (sr * 0.2));
        check (back < 1.0f, "al callar el sidechain se recupera (" + juce::String (back, 2) + " dB)");

        // Captura con una "voz" de prueba en el sidechain para que se vea la pantalla
        setParam (s, HuecoID::lowFreq, 120.0f);
        setParam (s, HuecoID::highFreq, 8000.0f);
        auto voice = sine ({ { 220.0, 0.25f }, { 440.0, 0.2f }, { 880.0, 0.25f }, { 1250.0, 0.2f }, { 2600.0, 0.1f }, { 5200.0, 0.04f } }, 1.0);
        auto pad = sine ({ { 110.0, 0.2f }, { 330.0, 0.2f }, { 990.0, 0.1f } }, 1.0);
        run (p, pad, &voice);
        snapshot (p, outDir.getChildFile ("hueco.png"));
    }

    std::printf ("%s\n", failures == 0 ? "TODO OK" : "HAY FALLOS");
    return failures == 0 ? 0 : 1;
}
