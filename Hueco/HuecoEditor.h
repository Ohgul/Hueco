#pragma once
#include "HuecoProcessor.h"
#include "../Common/FxLook.h"

// Pantalla: espectro del sidechain (barras) y el hueco que se está abriendo (curva)
class HuecoDisplay : public juce::Component
{
public:
    explicit HuecoDisplay (HuecoProcessor& p) : proc (p) { levels.fill (-100.0f); cuts.fill (0.0f); }
    void update();
    void paint (juce::Graphics&) override;

private:
    float xFor (float hz) const;
    HuecoProcessor& proc;
    std::array<float, HuecoEngine::numBands> levels, cuts;
};

class HuecoEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit HuecoEditor (HuecoProcessor&);
    ~HuecoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HuecoProcessor& proc;
    FxLook look { juce::Colour (0xff4cc9f0) };
    HuecoDisplay display;
    FxKnob amount, low, high, attack, release, mix;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    juce::Rectangle<int> displayPanel, knobPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HuecoEditor)
};
