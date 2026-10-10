#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Estilo visual compartido por los plugins (Lazo y Hueco).
namespace FxColours
{
    inline const juce::Colour bg       { 0xff17181c };
    inline const juce::Colour panel    { 0xff202227 };
    inline const juce::Colour edge     { 0xff30333a };
    inline const juce::Colour knobTop  { 0xff3a3d45 };
    inline const juce::Colour knobBot  { 0xff24262c };
    inline const juce::Colour text     { 0xffeceae4 };
    inline const juce::Colour dim      { 0xff9a978f };
}

class FxLook : public juce::LookAndFeel_V4
{
public:
    explicit FxLook (juce::Colour accentColour) : accent (accentColour)
    {
        using namespace FxColours;
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, dim);
        setColour (juce::ComboBox::backgroundColourId, knobBot);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, edge);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.45f));
        setColour (juce::TextButton::buttonColourId, knobBot);
        setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOffId, dim);
        setColour (juce::TextButton::textColourOnId, bg);
        setColour (juce::TextEditor::backgroundColourId, knobBot);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId, accent);
    }

    juce::Colour accent;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace FxColours;
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Marcas alrededor
        const bool stepped = slider.getInterval() >= 1.0 && (slider.getMaximum() - slider.getMinimum()) <= 12.0;
        const int ticks = stepped ? (int) (slider.getMaximum() - slider.getMinimum()) + 1 : 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
            const float r1 = radius - 1.0f, r2 = radius - (stepped ? 6.0f : 4.0f);
            g.setColour (dim.withAlpha (stepped ? 0.9f : 0.5f));
            g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), stepped ? 1.6f : 1.0f);
        }

        // Arco de valor
        const float arcR = radius - 9.0f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.9f));
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Perilla
        const float kr = arcR - 5.0f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - kr, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);
        g.setGradientFill (juce::ColourGradient (knobTop.brighter (slider.isMouseOverOrDragging() ? 0.15f : 0.0f), c.x, c.y - kr,
                                                 knobBot, c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (edge.brighter (0.3f));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

        // Indicador
        const juce::Point<float> p1 (c.x + kr * 0.35f * std::sin (angle), c.y - kr * 0.35f * std::cos (angle));
        const juce::Point<float> p2 (c.x + (kr - 4.0f) * std::sin (angle), c.y - (kr - 4.0f) * std::cos (angle));
        g.setColour (accent);
        g.drawLine ({ p1, p2 }, 3.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.isMouseOver() ? accent : FxColours::edge);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (accent);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (juce::FontOptions (13.0f));
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::FontOptions (12.5f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (down ? 0.7f : 0.9f) : FxColours::knobBot.brighter (over ? 0.1f : 0.0f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? accent : FxColours::edge);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
};

// Perilla con nombre arriba y valor abajo, conectada a un parámetro
class FxKnob : public juce::Component
{
public:
    FxKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
    {
        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
        if (auto* p = s.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Botón de encendido conectado a un parámetro booleano
class FxToggle : public juce::TextButton
{
public:
    FxToggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& buttonText)
        : juce::TextButton (buttonText)
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, *this);
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Medidor vertical simple (dB)
class FxBarMeter : public juce::Component
{
public:
    void setLevelDb (float db) { level = juce::jmax (db, level - 1.5f); repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (FxColours::knobBot);
        g.fillRoundedRectangle (r, 2.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (level + 48.0f) / 48.0f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * norm);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6fbf73), 0.0f, r.getBottom(),
                                                 juce::Colour (0xffe0533d), 0.0f, r.getY(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    }

private:
    float level = -60.0f;
};
