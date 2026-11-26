#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Aspetto in stile Crest Tamer: sfondo nero, pomelli con tacche azzurre e indicatore rosso, testi verde smeraldo.
class CrestLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrestLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Meter a segmenti (come nel programma): verde / giallo / rosso. fromTop = true per la riduzione (si riempie dall'alto).
class LevelBar : public juce::Component
{
public:
    LevelBar (juce::String titleText, float minValue, float maxValue, bool fillFromTop);
    void setValue (float v) { value = v; repaint(); }
    void paint (juce::Graphics&) override;
private:
    juce::String title;
    float minV, maxV, value;
    bool fromTop;
};

// Forma d'onda scorrevole (ultimi ~5 secondi): ingresso azzurro, uscita verde, soglia gialla (scende dove il
// plugin abbassa un picco), parti oltre la soglia in rosso. La rotella del mouse cambia lo zoom verticale.
class WaveView : public juce::Component
{
public:
    static constexpr int kCols = 1000;                         // 1000 colonne da 5 ms = 5 secondi
    void push (const crest::WaveColumn& c);
    void paint (juce::Graphics&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    std::array<crest::WaveColumn, kCols> cols {};
    int head = 0, count = 0;
    float zoom = 1.0f;
};

class CrestTamerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CrestTamerEditor (CrestTamerProcessor&);
    ~CrestTamerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CrestTamerProcessor& proc;
    CrestLookAndFeel laf;

    WaveView wave;
    juce::Slider inputSlider, thresholdSlider, qSlider, outputSlider;
    juce::Label inputLabel, thresholdLabel, qLabel, outputLabel;
    juce::ToggleButton linkButton { "Speculare" };
    LevelBar inBar  { "IN",  -60.0f, 0.0f, false };
    LevelBar grBar  { "GR",    0.0f, 12.0f, true };
    LevelBar outBar { "OUT", -60.0f, 0.0f, false };
    float inLevel = -100.0f, outLevel = -100.0f, grLevel = 0.0f;

    // gli "attachment" collegano i controlli ai parametri: vanno dichiarati dopo i controlli
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAtt> inputAtt, thresholdAtt, qAtt, outputAtt;
    std::unique_ptr<ButtonAtt> linkAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrestTamerEditor)
};
