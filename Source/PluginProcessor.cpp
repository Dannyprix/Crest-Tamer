#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout CrestTamerProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input",
                 NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold",
                 NormalisableRange<float> (3.0f, 30.0f, 0.1f), 12.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "q", 1 }, "Q",
                 NormalisableRange<float> (1.0f, 50.0f, 0.1f), 5.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                 NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "link", 1 }, "Speculare", true));
    return { p.begin(), p.end() };
}

CrestTamerProcessor::CrestTamerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    pInput     = apvts.getRawParameterValue ("input");
    pThreshold = apvts.getRawParameterValue ("threshold");
    pQ         = apvts.getRawParameterValue ("q");
    pOutput    = apvts.getRawParameterValue ("output");
    pLink      = apvts.getRawParameterValue ("link");
}

bool CrestTamerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void CrestTamerProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate, std::max (1, getTotalNumInputChannels()));
    dsp.setParameters (pThreshold->load(), pQ->load(), pInput->load(), pOutput->load(), pLink->load() < 0.5f);
    dsp.reset();
    setLatencySamples (dsp.latencySamples());      // 50 ms fissi: il programma ospite li compensa
}

void CrestTamerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    for (int c = nIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    const int nch = std::min (nIn, buffer.getNumChannels());
    float inPeak = 0.0f;
    for (int c = 0; c < nch; ++c)
        inPeak = std::max (inPeak, buffer.getMagnitude (c, 0, numSamples));

    dsp.setParameters (pThreshold->load(), pQ->load(), pInput->load(), pOutput->load(), pLink->load() < 0.5f);
    dsp.process (buffer.getArrayOfWritePointers(), nch, numSamples);

    float outPeak = 0.0f;
    for (int c = 0; c < nch; ++c)
        outPeak = std::max (outPeak, buffer.getMagnitude (c, 0, numSamples));

    inPeakDb.store  (juce::Decibels::gainToDecibels (inPeak,  -100.0f));
    outPeakDb.store (juce::Decibels::gainToDecibels (outPeak, -100.0f));
    reductionDb.store (-juce::Decibels::gainToDecibels (dsp.lastBlockMinGain(), -100.0f));
}

juce::AudioProcessorEditor* CrestTamerProcessor::createEditor() { return new CrestTamerEditor (*this); }

void CrestTamerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CrestTamerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CrestTamerProcessor(); }
