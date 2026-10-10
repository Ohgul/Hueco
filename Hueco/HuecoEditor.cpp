#include "HuecoEditor.h"

using namespace FxColours;

static const juce::Colour scColour  { 0xff4cc9f0 };   // sidechain
static const juce::Colour cutColour { 0xffff7a6b };   // hueco

float HuecoDisplay::xFor (float hz) const
{
    const float lo = std::log (25.0f), hi = std::log (20000.0f);
    return (float) getWidth() * (std::log (juce::jlimit (25.0f, 20000.0f, hz)) - lo) / (hi - lo);
}

void HuecoDisplay::update()
{
    for (int k = 0; k < HuecoEngine::numBands; ++k)
    {
        const float l = proc.engine.displayLevel[(size_t) k].load();
        const float c = proc.engine.displayCut[(size_t) k].load();
        levels[(size_t) k] = l > levels[(size_t) k] ? l : levels[(size_t) k] - 1.2f;   // caída suave
        cuts[(size_t) k] += (c - cuts[(size_t) k]) * 0.35f;
    }
    repaint();
}

void HuecoDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff121316));
    g.fillRoundedRectangle (r, 6.0f);

    // Rejilla de frecuencias
    g.setFont (juce::FontOptions (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xFor (hz);
        g.setColour (edge.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, r.getY() + 4.0f, r.getBottom() - 16.0f);
        g.setColour (dim.withAlpha (0.8f));
        g.drawText (hz < 1000.0f ? juce::String ((int) hz) : juce::String ((int) (hz / 1000.0f)) + "k",
                    juce::Rectangle<float> (x - 20.0f, r.getBottom() - 15.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    // Zona fuera del rango (atenuada)
    const float lowX = xFor (proc.state.getRawParameterValue (HuecoID::lowFreq)->load());
    const float highX = xFor (proc.state.getRawParameterValue (HuecoID::highFreq)->load());
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), lowX - r.getX(), r.getHeight() - 16.0f));
    g.fillRect (juce::Rectangle<float> (highX, r.getY(), r.getRight() - highX, r.getHeight() - 16.0f));

    const float top = r.getY() + 8.0f, bottom = r.getBottom() - 20.0f, h = bottom - top;

    // Barras del sidechain (de abajo hacia arriba)
    for (int k = 0; k < HuecoEngine::numBands; ++k)
    {
        const float f = HuecoEngine::bandFreq (k);
        const float x0 = xFor (f / 1.1f), x1 = xFor (f * 1.1f);
        const float norm = juce::jlimit (0.0f, 1.0f, (levels[(size_t) k] + 60.0f) / 60.0f);
        auto bar = juce::Rectangle<float> (x0 + 1.0f, bottom - h * norm, juce::jmax (2.0f, x1 - x0 - 2.0f), h * norm);
        g.setGradientFill (juce::ColourGradient (scColour.withAlpha (0.65f), 0.0f, bar.getY(), scColour.withAlpha (0.12f), 0.0f, bottom, false));
        g.fillRoundedRectangle (bar, 2.0f);
    }

    // Curva del hueco (desde arriba hacia abajo: lo que se está quitando)
    juce::Path curve;
    curve.startNewSubPath (r.getX(), top);
    for (int k = 0; k < HuecoEngine::numBands; ++k)
    {
        const float x = xFor (HuecoEngine::bandFreq (k));
        const float y = top + h * juce::jlimit (0.0f, 1.0f, cuts[(size_t) k] / HuecoEngine::maxCutDb);
        curve.lineTo (x, y);
    }
    curve.lineTo (r.getRight(), top);
    auto filled = curve;
    filled.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cutColour.withAlpha (0.45f), 0.0f, top, cutColour.withAlpha (0.05f), 0.0f, bottom, false));
    g.fillPath (filled);
    g.setColour (cutColour);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Escala de corte
    g.setColour (dim);
    g.setFont (juce::FontOptions (10.0f));
    for (int db : { 0, 10, 20, 30 })
        g.drawText ("-" + juce::String (db) + " dB", juce::Rectangle<float> (r.getRight() - 48.0f, top + h * db / 30.0f - 6.0f, 44.0f, 12.0f),
                    juce::Justification::centredRight);

    // Leyenda
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (scColour);
    g.drawText ("SIDECHAIN", (int) r.getX() + 10, (int) r.getY() + 8, 90, 14, juce::Justification::centredLeft);
    g.setColour (cutColour);
    g.drawText ("HUECO", (int) r.getX() + 100, (int) r.getY() + 8, 60, 14, juce::Justification::centredLeft);

    // Aviso si no llega nada por sidechain
    if (proc.sidechainLevel.load() < -80.0f)
    {
        g.setColour (text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText ("No llega senal por sidechain.\nEn Ableton, abre el menu \"Sidechain\" del plugin y elige la pista que debe tener prioridad (voz, bombo...).",
                          getLocalBounds().reduced (60, 50), juce::Justification::centred, 3);
    }
}

//==============================================================================
HuecoEditor::HuecoEditor (HuecoProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p),
      amount (p.state, HuecoID::amount, "Cantidad"), low (p.state, HuecoID::lowFreq, "Desde"),
      high (p.state, HuecoID::highFreq, "Hasta"),     attack (p.state, HuecoID::attack, "Ataque"),
      release (p.state, HuecoID::release, "Liberacion"), mix (p.state, HuecoID::mix, "Mezcla")
{
    setLookAndFeel (&look);
    addAndMakeVisible (display);
    for (auto* k : { &amount, &low, &high, &attack, &release, &mix })
        addAndMakeVisible (k);

    modeBox.addItemList (huecoModeNames(), 1);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.state, HuecoID::mode, modeBox);
    addAndMakeVisible (modeBox);
    modeLabel.setText ("MODO", juce::dontSendNotification);
    modeLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    modeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modeLabel);
    modeBox.setTooltip ("Estereo: igual en ambos lados. Izq/Der: cada lado por separado. Medio/Lados: hueco solo en el centro o solo en los lados.");

    setSize (860, 470);
    startTimerHz (30);
}

HuecoEditor::~HuecoEditor() { setLookAndFeel (nullptr); }

void HuecoEditor::timerCallback() { display.update(); }

void HuecoEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (look.accent);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("HUECO", 22, 10, 130, 36, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText ("espacio espectral por sidechain", 132, 20, 260, 20, juce::Justification::centredLeft);

    for (auto area : { displayPanel, knobPanel })
    {
        g.setColour (panel);
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }
}

void HuecoEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 0);
    r.removeFromTop (56);
    r.removeFromBottom (16);
    knobPanel = r.removeFromBottom (150);
    r.removeFromBottom (12);
    displayPanel = r;
    display.setBounds (displayPanel.reduced (10));

    auto k = knobPanel.reduced (10);
    auto modeArea = k.removeFromRight (150);
    modeLabel.setBounds (modeArea.removeFromTop (16).translated (0, 6));
    modeBox.setBounds (modeArea.withSizeKeepingCentre (130, 28));
    const int w = k.getWidth() / 6;
    for (auto* knob : { &amount, &low, &high, &attack, &release, &mix })
        knob->setBounds (k.removeFromLeft (w).reduced (4, 0));
}
