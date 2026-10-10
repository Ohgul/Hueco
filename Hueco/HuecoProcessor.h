#pragma once
#include "HuecoDSP.h"

class HuecoProcessor : public juce::AudioProcessor
{
public:
    HuecoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Hueco"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    HuecoEngine engine;

    std::atomic<float> sidechainLevel { -100.0f };   // dBFS del sidechain (para avisar si no llega nada)

private:
    juce::AudioBuffer<float> monoTmp;
    std::atomic<float> *pAmount, *pLow, *pHigh, *pAtk, *pRel, *pMix, *pMode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HuecoProcessor)
};
