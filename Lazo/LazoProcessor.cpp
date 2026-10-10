#include "LazoProcessor.h"
#include "LazoEditor.h"

LazoProcessor::LazoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "LazoState", createLazoLayout())
{
    auto g = [this] (const char* id) { return state.getRawParameterValue (id); };
    pThr = g (LazoID::threshold); pRatio = g (LazoID::ratio); pAtk = g (LazoID::attack); pRel = g (LazoID::release);
    pMakeup = g (LazoID::makeup); pMix = g (LazoID::mix); pHpf = g (LazoID::scHpf); pRange = g (LazoID::range);
    pClip = g (LazoID::clip); pExt = g (LazoID::extSc);
    state.state.setProperty ("preset", 0, nullptr);
}

bool LazoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void LazoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    comp.prepare (sampleRate);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));
    scCopy.setSize (2, samplesPerBlock);
}

void LazoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();
    const int numCh = main.getNumChannels();
    if (n == 0 || numCh == 0) return;

    float* l = main.getWritePointer (0);
    float* r = main.getWritePointer (numCh > 1 ? 1 : 0);

    meterIn = juce::Decibels::gainToDecibels (main.getMagnitude (0, n), -60.0f);

    // Cadena lateral externa (si está conectada y activada)
    const float* scL = nullptr;
    const float* scR = nullptr;
    const bool hasSc = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
    sidechainConnected = hasSc;
    if (hasSc && pExt->load() > 0.5f)
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scCopy.setSize (2, n, false, false, true);
            scCopy.copyFrom (0, 0, sc, 0, 0, n);
            scCopy.copyFrom (1, 0, sc, sc.getNumChannels() > 1 ? 1 : 0, 0, n);
            scL = scCopy.getReadPointer (0);
            scR = scCopy.getReadPointer (1);
        }
    }

    LazoCompressor::Settings s;
    s.thresholdDb = pThr->load();
    s.ratio       = pRatio->load();
    s.attackMs    = LazoTables::attackMs[juce::jlimit (0, 5, (int) pAtk->load())];
    s.releaseS    = LazoTables::releaseS[juce::jlimit (0, 6, (int) pRel->load())];
    s.makeupDb    = pMakeup->load();
    s.mix         = pMix->load();
    s.scHpfHz     = pHpf->load();
    s.rangeDb     = pRange->load();

    const float gr = comp.process (l, r, scL, scR, n, s);

    // Clip suave a doble resolución (siempre pasa por el sobremuestreo para latencia constante)
    {
        juce::dsp::AudioBlock<float> block (main);
        auto up = oversampling.processSamplesUp (block);
        if (pClip->load() > 0.5f)
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = LazoCompressor::softClip (d[i]);
            }
        oversampling.processSamplesDown (block);
    }

    meterGr  = juce::jmax (gr, meterGr.load() * 0.9f);
    meterOut = juce::Decibels::gainToDecibels (main.getMagnitude (0, n), -60.0f);
}

void LazoProcessor::loadPreset (int index)
{
    const auto& list = lazoPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& p = list[(size_t) index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = state.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set (LazoID::threshold, p.thr);  set (LazoID::ratio, p.ratio);
    set (LazoID::attack, p.atk);     set (LazoID::release, p.rel);
    set (LazoID::makeup, p.makeup);  set (LazoID::mix, p.mix);
    set (LazoID::scHpf, p.hpf);      set (LazoID::range, p.range);
    set (LazoID::clip, p.clip ? 1.0f : 0.0f);
    state.state.setProperty ("preset", index, nullptr);
}

void LazoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void LazoProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* LazoProcessor::createEditor() { return new LazoEditor (*this); }

#ifndef REMACDENT_TEST
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LazoProcessor(); }
#endif
