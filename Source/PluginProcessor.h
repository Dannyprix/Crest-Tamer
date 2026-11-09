#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "CrestTamerDSP.h"

// Plugin Crest Tamer: riduce i picchi senza compressore né limiter (vedi CrestTamerDSP.h).
// Parametri:  input (dB), threshold (crest factor, dB), q (larghezza ritocco = 50/Q ms), output (dB),
//             link ("Speculare": acceso = un solo guadagno, spento = sopra/sotto lo zero indipendenti).
class CrestTamerProcessor : public juce::AudioProcessor
{
public:
    CrestTamerProcessor();
    ~CrestTamerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Crest Tamer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // valori letti dall'editor per i meter (aggiornati ogni blocco, dal thread audio)
    std::atomic<float> reductionDb { 0.0f };    // riduzione massima applicata nell'ultimo blocco (dB, >= 0)
    std::atomic<float> inPeakDb    { -100.0f };
    std::atomic<float> outPeakDb   { -100.0f };

private:
    crest::CrestTamerDSP dsp;
    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pQ = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pLink = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrestTamerProcessor)
};
