#pragma once
#include "LazoDSP.h"

class LazoProcessor : public juce::AudioProcessor
{
public:
    LazoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Lazo"; }
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

    void loadPreset (int index);
    int getPresetIndex() const { return (int) state.state.getProperty ("preset", 0); }

    juce::AudioProcessorValueTreeState state;

    // Para los medidores de la interfaz
    std::atomic<float> meterGr { 0.0f }, meterIn { -60.0f }, meterOut { -60.0f };
    std::atomic<bool> sidechainConnected { false };

private:
    LazoCompressor comp;
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::AudioBuffer<float> scCopy;

    std::atomic<float> *pThr, *pRatio, *pAtk, *pRel, *pMakeup, *pMix, *pHpf, *pRange, *pClip, *pExt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LazoProcessor)
};
