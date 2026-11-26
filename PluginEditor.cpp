#include "PluginEditor.h"

namespace
{
const juce::Colour kEmerald { 0xff50C878 }, kText { 0xffc9ced6 }, kBlue { 0xff4aa3ff },
                   kRed { 0xffff4d4d }, kAmber { 0xffffc247 }, kGrey { 0xff6b7280 },
                   kYellow { 0xffffd400 }, kGreenWave { 0xff4cd38a };
}

// ----------------------------------------------------------------------------- aspetto
CrestLookAndFeel::CrestLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kEmerald);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
}

void CrestLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float a0, float a1, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    const float side = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float angle = a0 + pos * (a1 - a0);

    constexpr int nTicks = 28;                                    // tacche: azzurre fino al valore, grigie dopo
    for (int i = 0; i <= nTicks; ++i)
    {
        const float t = (float) i / (float) nTicks, ang = a0 + t * (a1 - a0);
        const bool major = (i % 4) == 0;
        g.setColour (t <= pos + 1.0e-4f ? kBlue : kGrey);
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (side * (major ? 0.37f : 0.40f), ang),
                                       c.getPointOnCircumference (side * 0.47f, ang)), major ? 2.5f : 1.2f);
    }

    const float r = side * 0.31f;                                 // corpo del pomello
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a505c), c.x - r * 0.3f, c.y - r * 0.3f,
                                             juce::Colour (0xff1d2027), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 2.0f);

    juce::Path ind;                                               // indicatore rosso
    ind.startNewSubPath (c.getPointOnCircumference (r * 0.30f, angle));
    ind.lineTo (c.getPointOnCircumference (r * 0.88f, angle));
    g.setColour (kRed);
    g.strokePath (ind, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CrestLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? kEmerald.withAlpha (highlighted ? 0.9f : 0.75f) : juce::Colour (0xff1a1c21));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? kEmerald : kGrey);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::black : kText);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (b.getButtonText() + (on ? ": ON" : ": OFF"), b.getLocalBounds(), juce::Justification::centred);
}

// ----------------------------------------------------------------------------- meter
LevelBar::LevelBar (juce::String titleText, float minValue, float maxValue, bool fillFromTop)
    : title (std::move (titleText)), minV (minValue), maxV (maxValue), value (minValue), fromTop (fillFromTop) {}

void LevelBar::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (kText);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (title, area.removeFromTop (16.0f), juce::Justification::centred);
    auto text = area.removeFromBottom (16.0f);
    g.setColour (kEmerald);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (fromTop ? juce::String (value, 1) + " dB" : (value <= minV + 0.05f ? juce::String ("–∞") : juce::String (value, 1)),
                text, juce::Justification::centred);

    constexpr int nSeg = 30;
    const float segH = (area.getHeight() - (nSeg - 1)) / (float) nSeg;
    const float frac = juce::jlimit (0.0f, 1.0f, (value - minV) / (maxV - minV));
    for (int i = 0; i < nSeg; ++i)
    {
        const float t = ((float) i + 0.5f) / (float) nSeg;               // posizione del segmento 0..1 (dal basso, o dall'alto)
        const bool lit = t <= frac + 0.5f / (float) nSeg;
        const float level = fromTop ? t : t;                              // per colore: più in alto/lontano = più "caldo"
        const juce::Colour on = level > 0.93f ? kRed : level > 0.80f ? kAmber : kEmerald;
        const float y = fromTop ? area.getY() + (float) i * (segH + 1.0f)
                                : area.getBottom() - (float) (i + 1) * segH - (float) i;
        g.setColour (lit ? on : on.withBrightness (0.18f));
        g.fillRect (area.getX() + 2.0f, y, area.getWidth() - 4.0f, segH);
    }
}

// ----------------------------------------------------------------------------- forma d'onda
void WaveView::push (const crest::WaveColumn& c)
{
    cols[(size_t) head] = c;
    head = (head + 1) % kCols;
    count = juce::jmin (count + 1, kCols);
}

void WaveView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& d)
{
    zoom = juce::jlimit (1.0f, 16.0f, zoom * std::pow (1.25f, d.deltaY * 4.0f));
    repaint();
}

void WaveView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0a0c));
    g.fillRect (r);
    const float cy = r.getCentreY(), half = r.getHeight() * 0.5f - 3.0f;
    auto Y = [&] (float v) { return cy - juce::jlimit (-1.0f, 1.0f, v * zoom) * half; };

    g.setColour (juce::Colour (0xff1d2027));                      // griglia: 0 e ±0,5
    for (float v : { -0.5f, 0.0f, 0.5f })
        g.drawHorizontalLine ((int) Y (v), r.getX(), r.getRight());

    const float colW = r.getWidth() / (float) kCols;
    juce::Path yellowP, yellowN;
    for (int i = 0; i < count; ++i)
    {
        const auto& c = cols[(size_t) ((head - count + i + 2 * kCols) % kCols)];
        const float x = r.getX() + ((float) (kCols - count + i) + 0.5f) * colW;
        g.setColour (kBlue);                                      // ingresso
        g.drawLine (x, Y (c.inMax), x, Y (c.inMin), 1.0f);
        g.setColour (kRed);                                       // oltre la soglia
        if (c.inMax > c.ceil) g.drawLine (x, Y (c.ceil), x, Y (c.inMax), 1.6f);
        if (c.inMin < -c.ceil) g.drawLine (x, Y (-c.ceil), x, Y (c.inMin), 1.6f);
        g.setColour (kGreenWave.withAlpha (0.9f));                // uscita elaborata
        g.drawLine (x, Y (c.outMax), x, Y (c.outMin), 1.0f);
        const float yp = Y (juce::jmin (c.ceil * c.gMinP, 1.0f)), yn = Y (-juce::jmin (c.ceil * c.gMinN, 1.0f));
        if (i == 0) { yellowP.startNewSubPath (x, yp); yellowN.startNewSubPath (x, yn); }
        else        { yellowP.lineTo (x, yp); yellowN.lineTo (x, yn); }
    }
    g.setColour (kYellow);                                        // soglia: segue le correzioni
    g.strokePath (yellowP, juce::PathStrokeType (1.0f));
    g.strokePath (yellowN, juce::PathStrokeType (1.0f));

    g.setColour (kGrey);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("azzurro = ingresso   verde = uscita   giallo = soglia   rosso = oltre la soglia   (rotella = zoom"
                + (zoom > 1.01f ? " x" + juce::String (zoom, 1) : juce::String()) + ")",
                getLocalBounds().reduced (8, 4), juce::Justification::topLeft);
    g.setColour (juce::Colour (0xff2c3038));
    g.drawRect (r, 1.0f);
}

// ----------------------------------------------------------------------------- editor
CrestTamerEditor::CrestTamerEditor (CrestTamerProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    auto setup = [this] (juce::Slider& s, juce::Label& l, const juce::String& title, double def, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 120, 20);
        s.setNumDecimalPlacesToDisplay (1);
        s.setTextValueSuffix (suffix);
        s.setDoubleClickReturnValue (true, def);
        s.setMouseDragSensitivity (250);
        addAndMakeVisible (s);
        l.setText (title, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };
    setup (inputSlider,     inputLabel,     "INPUT",     0.0,  " dB");
    setup (thresholdSlider, thresholdLabel, "THRESHOLD", 12.0, " dB");
    setup (qSlider,         qLabel,         "Q",         5.0,  "");
    setup (outputSlider,    outputLabel,    "OUTPUT",    0.0,  " dB");

    // Q: oltre al valore mostra la larghezza del ritocco in millisecondi (50 / Q)
    qSlider.textFromValueFunction = [] (double v) { return "Q " + juce::String (v, 1) + "  (" + juce::String (50.0 / juce::jmax (v, 1.0), 1) + " ms)"; };
    qSlider.valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("0123456789.").getDoubleValue(); };

    addAndMakeVisible (wave);
    addAndMakeVisible (linkButton);
    linkButton.setTooltip ("Acceso: un solo guadagno per tutta l'onda. Spento: picchi sopra e sotto lo zero indipendenti.");
    addAndMakeVisible (inBar);
    addAndMakeVisible (grBar);
    addAndMakeVisible (outBar);

    inputAtt     = std::make_unique<SliderAtt> (proc.apvts, "input",     inputSlider);
    thresholdAtt = std::make_unique<SliderAtt> (proc.apvts, "threshold", thresholdSlider);
    qAtt         = std::make_unique<SliderAtt> (proc.apvts, "q",         qSlider);
    outputAtt    = std::make_unique<SliderAtt> (proc.apvts, "output",    outputSlider);
    linkAtt      = std::make_unique<ButtonAtt> (proc.apvts, "link",      linkButton);

    setSize (760, 500);
    startTimerHz (30);
}

CrestTamerEditor::~CrestTamerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CrestTamerEditor::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;                         // ricaduta dolce dei meter
    inLevel  = juce::jmax (proc.inPeakDb.load(),  inLevel  - 40.0f * dt);
    outLevel = juce::jmax (proc.outPeakDb.load(), outLevel - 40.0f * dt);
    grLevel  = juce::jmax (proc.reductionDb.load(), grLevel - 20.0f * dt);
    crest::WaveColumn col;
    bool any = false;
    while (proc.popWaveColumn (col)) { wave.push (col); any = true; }
    if (any) wave.repaint();
    inBar.setValue (inLevel);
    outBar.setValue (outLevel);
    grBar.setValue (grLevel);
}

void CrestTamerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (kEmerald);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("CREST TAMER", 20, 10, 300, 28, juce::Justification::centredLeft);
    g.setColour (kGrey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("riduzione dei picchi senza compressore né limiter", 20, 36, 380, 18, juce::Justification::centredLeft);
}

void CrestTamerEditor::resized()
{
    const int margin = 24;
    wave.setBounds (margin, 62, getWidth() - 2 * margin, 190);              // forma d'onda a tutta larghezza

    const int knob = 120, gap = 14, y0 = 306;                                // pomelli
    juce::Slider* sliders[] = { &inputSlider, &thresholdSlider, &qSlider, &outputSlider };
    juce::Label* labels[]   = { &inputLabel, &thresholdLabel, &qLabel, &outputLabel };
    for (int i = 0; i < 4; ++i)
    {
        const int x = margin + i * (knob + gap);
        labels[i]->setBounds (x, y0 - 24, knob, 20);
        sliders[i]->setBounds (x, y0, knob, knob + 24);                      // pomello + casella del valore sotto
    }
    const int knobsEnd = margin + 4 * knob + 3 * gap;                        // fine dell'ultimo pomello

    // meter: a destra dei pomelli con un margine netto (e uno dal bordo della finestra)
    const int barW = 28, barGap = 10, barsW = 3 * barW + 2 * barGap;
    const int barsX = getWidth() - margin - barsW;
    jassert (barsX > knobsEnd + 16);
    juce::ignoreUnused (knobsEnd);                                           // (usata solo dal controllo qui sopra)
    inBar.setBounds  (barsX, y0 - 24, barW, 170);
    grBar.setBounds  (barsX + barW + barGap, y0 - 24, barW, 170);
    outBar.setBounds (barsX + 2 * (barW + barGap), y0 - 24, barW, 170);

    linkButton.setBounds (margin, getHeight() - 52, 160, 30);
}
