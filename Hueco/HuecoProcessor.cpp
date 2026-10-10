#include "HuecoProcessor.h"
#include "HuecoEditor.h"

HuecoProcessor::HuecoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "HuecoState", createHuecoLayout())
{
    auto g = [this] (const char* id) { return state.getRawParameterValue (id); };
    pAmount = g (HuecoID::amount); pLow = g (HuecoID::lowFreq); pHigh = g (HuecoID::highFreq);
    pAtk = g (HuecoID::attack); pRel = g (HuecoID::release); pMix = g (HuecoID::mix); pMode = g (HuecoID::mode);
}

bool HuecoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono())
            return false;
    }
    return true;
}

void HuecoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    monoTmp.setSize (1, samplesPerBlock);
}

void HuecoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();
    if (n == 0 || main.getNumChannels() == 0) return;

    const float* scL = nullptr;
    const float* scR = nullptr;
    float scMag = 0.0f;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getReadPointer (sc.getNumChannels() > 1 ? 1 : 0);
            scMag = sc.getMagnitude (0, n);
        }
    }
    sidechainLevel = juce::jmax (juce::Decibels::gainToDecibels (scMag, -100.0f), sidechainLevel.load() - 2.0f);

    HuecoEngine::Settings s;
    s.amount = pAmount->load();
    s.lowHz = pLow->load();
    s.highHz = juce::jmax (pHigh->load(), s.lowHz + 10.0f);
    s.attackMs = pAtk->load();
    s.releaseMs = pRel->load();
    s.mix = pMix->load();
    s.mode = (int) pMode->load();

    float* L = main.getWritePointer (0);
    float* R;
    if (main.getNumChannels() > 1)
        R = main.getWritePointer (1);
    else
    {
        monoTmp.setSize (1, n, false, false, true);
        monoTmp.copyFrom (0, 0, main, 0, 0, n);
        R = monoTmp.getWritePointer (0);
        s.mode = 0;
    }
    engine.process (L, R, scL, scR, n, s);
}

void HuecoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void HuecoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HuecoProcessor::createEditor() { return new HuecoEditor (*this); }

#ifndef REMACDENT_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HuecoProcessor(); }
#endif
