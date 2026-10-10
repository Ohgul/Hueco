#include "LazoEditor.h"

using namespace FxColours;

void GrNeedle::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    // Cara del medidor
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3e7cf), r.getCentreX(), r.getY(),
                                             juce::Colour (0xffd9c7a3), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.5f);

    const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - 18.0f);
    const float radius = juce::jmin (r.getWidth() * 0.46f, r.getHeight() - 34.0f);
    const float a0 = -0.8f, a1 = 0.8f;            // ángulos extremos (radianes desde la vertical)
    auto angleFor = [&] (float db) { return a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, std::sqrt (db / 20.0f)); };
    // Escala: 0 a la derecha (sin reducción) y 20 dB a la izquierda
    auto toAngle = [&] (float db) { return -angleFor (db) ; };

    // Zona roja (más de 10 dB)
    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, radius - 6.0f, radius - 6.0f, 0.0f, toAngle (20.0f), toAngle (10.0f), true);
    g.setColour (juce::Colour (0xffc0392b).withAlpha (0.8f));
    g.strokePath (red, juce::PathStrokeType (5.0f));

    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, toAngle (20.0f), toAngle (0.0f), true);
    g.setColour (juce::Colour (0xff2b2722));
    g.strokePath (arc, juce::PathStrokeType (1.2f));

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (float db : { 0.0f, 1.0f, 2.0f, 4.0f, 6.0f, 10.0f, 14.0f, 20.0f })
    {
        const float a = toAngle (db);
        const juce::Point<float> p1 (pivot.x + radius * std::sin (a), pivot.y - radius * std::cos (a));
        const juce::Point<float> p2 (pivot.x + (radius + 7.0f) * std::sin (a), pivot.y - (radius + 7.0f) * std::cos (a));
        g.setColour (juce::Colour (0xff2b2722));
        g.drawLine ({ p1, p2 }, 1.4f);
        const juce::Point<float> tp (pivot.x + (radius + 17.0f) * std::sin (a), pivot.y - (radius + 17.0f) * std::cos (a));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (tp.x - 12.0f, tp.y - 7.0f, 24.0f, 14.0f), juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff2b2722).withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("REDUCCION  dB", juce::Rectangle<float> (r.getX(), pivot.y - radius * 0.45f, r.getWidth(), 14.0f), juce::Justification::centred);

    // Aguja
    const float a = toAngle (shown);
    const juce::Point<float> tip (pivot.x + (radius + 2.0f) * std::sin (a), pivot.y - (radius + 2.0f) * std::cos (a));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (pivot.x + 2.0f, pivot.y + 2.0f, tip.x + 2.0f, tip.y + 2.0f, 2.2f);
    g.setColour (juce::Colour (0xff1c1a17));
    g.drawLine ({ pivot, tip }, 2.0f);
    g.setColour (accent.darker (0.3f));
    g.fillEllipse (pivot.x - 6.0f, pivot.y - 6.0f, 12.0f, 12.0f);
}

//==============================================================================
LazoEditor::LazoEditor (LazoProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      threshold (p.state, LazoID::threshold, "Umbral"), ratio (p.state, LazoID::ratio, "Ratio"),
      attack (p.state, LazoID::attack, "Ataque"),       release (p.state, LazoID::release, "Liberacion"),
      makeup (p.state, LazoID::makeup, "Ganancia"),     mix (p.state, LazoID::mix, "Mezcla"),
      hpf (p.state, LazoID::scHpf, "Filtro SC"),        range (p.state, LazoID::range, "Rango"),
      clip (p.state, LazoID::clip, "CLIP"),             extSc (p.state, LazoID::extSc, "SC EXTERNO")
{
    setLookAndFeel (&look);

    addAndMakeVisible (needle);
    for (auto* k : { &threshold, &ratio, &attack, &release, &makeup, &mix, &hpf, &range })
        addAndMakeVisible (k);
    for (auto* b : { &clip, &extSc })
        addAndMakeVisible (b);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    clip.setTooltip ("Recortador suave a la salida: redondea los picos sin pasar de 0 dB");
    extSc.setTooltip ("Usa otra pista (sidechain) para controlar la compresion");

    const auto& presets = lazoPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.loadPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    auto step = [this] (int d)
    {
        const int n = presetBox.getNumItems();
        presetBox.setSelectedItemIndex ((juce::jmax (0, presetBox.getSelectedItemIndex()) + d + n) % n, juce::sendNotification);
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    setSize (860, 420);
    startTimerHz (30);
}

LazoEditor::~LazoEditor() { setLookAndFeel (nullptr); }

void LazoEditor::timerCallback()
{
    needle.setGr (proc.meterGr.load());
    proc.meterGr = proc.meterGr.load() * 0.6f;
    inMeter.setLevelDb (proc.meterIn.load());
    outMeter.setLevelDb (proc.meterOut.load());
    repaint (meterPanel.getX(), meterPanel.getBottom() - 70, meterPanel.getWidth(), 70);
}

void LazoEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (look.accent);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("LAZO", 22, 10, 120, 36, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText ("compresor de bus", 108, 20, 200, 20, juce::Justification::centredLeft);

    for (auto area : { meterPanel, knobPanel })
    {
        g.setColour (panel);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    // Estado del sidechain externo
    if (proc.state.getRawParameterValue (LazoID::extSc)->load() > 0.5f)
    {
        const bool ok = proc.sidechainConnected.load();
        g.setColour (ok ? look.accent : juce::Colour (0xffe0533d));
        g.setFont (juce::FontOptions (11.5f));
        g.drawFittedText (ok ? "Sidechain externo activo" : "Elige la pista de sidechain en Ableton\n(menu desplegable del plugin)",
                          meterPanel.withTrimmedTop (meterPanel.getHeight() - 44).reduced (12, 4), juce::Justification::centred, 2);
    }

    // Etiquetas de los medidores de entrada/salida
    g.setColour (dim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("IN", inMeter.getX() - 6, inMeter.getBottom() + 4, inMeter.getWidth() + 12, 12, juce::Justification::centred);
    g.drawText ("OUT", outMeter.getX() - 8, outMeter.getBottom() + 4, outMeter.getWidth() + 16, 12, juce::Justification::centred);
}

void LazoEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 0);
    auto header = r.removeFromTop (56);
    auto pa = header.removeFromRight (290).withSizeKeepingCentre (290, 28);
    prevButton.setBounds (pa.removeFromLeft (28));
    nextButton.setBounds (pa.removeFromRight (28));
    presetBox.setBounds (pa.reduced (6, 0));

    r.removeFromBottom (16);
    meterPanel = r.removeFromLeft (310);
    r.removeFromLeft (14);
    knobPanel = r;

    auto m = meterPanel.reduced (14);
    needle.setBounds (m.removeFromTop (190));
    m.removeFromTop (14);
    auto buttons = m.removeFromTop (34);
    clip.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (4, 0));
    extSc.setBounds (buttons.reduced (4, 0));

    auto k = knobPanel.reduced (12);
    auto meters = k.removeFromRight (44);
    meters.removeFromBottom (18);
    inMeter.setBounds (meters.removeFromLeft (14).withTrimmedTop (18));
    meters.removeFromLeft (8);
    outMeter.setBounds (meters.removeFromLeft (14).withTrimmedTop (18));

    const int rowH = k.getHeight() / 2;
    auto row1 = k.removeFromTop (rowH), row2 = k;
    const int w = row1.getWidth() / 4;
    for (auto* knob : { &threshold, &ratio, &attack, &release }) knob->setBounds (row1.removeFromLeft (w).reduced (4));
    for (auto* knob : { &makeup, &mix, &hpf, &range })          knob->setBounds (row2.removeFromLeft (w).reduced (4));
}
