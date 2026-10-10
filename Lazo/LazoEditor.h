#pragma once
#include "LazoProcessor.h"
#include "../Common/FxLook.h"

// Medidor de aguja de reducción de ganancia
class GrNeedle : public juce::Component
{
public:
    explicit GrNeedle (juce::Colour a) : accent (a) {}
    void setGr (float db)
    {
        // Balística de aguja (sube rápido, baja suave)
        shown += (db - shown) * (db > shown ? 0.5f : 0.18f);
        repaint();
    }
    void paint (juce::Graphics&) override;

private:
    juce::Colour accent;
    float shown = 0.0f;
};

class LazoEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit LazoEditor (LazoProcessor&);
    ~LazoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LazoProcessor& proc;
    FxLook look { juce::Colour (0xfff2a541) };

    GrNeedle needle { juce::Colour (0xfff2a541) };
    FxKnob threshold, ratio, attack, release, makeup, mix, hpf, range;
    FxToggle clip, extSc;
    FxBarMeter inMeter, outMeter;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::Rectangle<int> meterPanel, knobPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LazoEditor)
};
