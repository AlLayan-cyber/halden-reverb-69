#include "PluginEditor.h"
#include <cmath>

HaldenLookAndFeel::HaldenLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff191c22));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff262a32));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9aa1ad));
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xfffef3c7));
}

void HaldenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto name = slider.getName();
    const bool isBig = (name == "preDelayMs" || name == "decay" || name == "mix");

    const float cx = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float outerR = static_cast<float> (juce::jmin (width, height)) * (isBig ? 0.36f : 0.37f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Printed tick marks directly on brushed faceplate
    const int ticks = isBig ? 21 : 11;
    const float tickIn = outerR + (isBig ? 3.5f : 2.5f);
    for (int i = 0; i < ticks; ++i)
    {
        const float tNorm = static_cast<float> (i) / static_cast<float> (ticks - 1);
        const float ta = rotaryStartAngle + tNorm * (rotaryEndAngle - rotaryStartAngle) - 1.5707963f;
        const bool isMajor = (i == 0 || i == ticks - 1 || i == (ticks - 1) / 2);
        const float tickOut = tickIn + (isMajor ? (isBig ? 6.0f : 4.2f) : (isBig ? 3.5f : 2.5f));
        g.setColour ((isBig && tNorm <= sliderPos + 0.015f) ? juce::Colour (0xfff59e0b)
                     : isMajor ? juce::Colour (0xffcfd5df) : juce::Colour (0xff6f7785));
        g.drawLine (cx + tickIn * std::cos (ta), cy + tickIn * std::sin (ta),
                    cx + tickOut * std::cos (ta), cy + tickOut * std::sin (ta),
                    isMajor ? 1.4f : 0.9f);
    }

    // Contact drop shadow
    g.setColour (juce::Colour (0xb0000000));
    g.fillEllipse (cx - outerR, cy - outerR + (isBig ? 4.0f : 2.5f), outerR * 2.0f, outerR * 2.0f);

    // Knurled machined edge
    const int teeth = isBig ? 32 : 20;
    juce::Path knurl;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float a = angle + static_cast<float> (i) * (3.14159265f / static_cast<float> (teeth));
        const float r = (i % 2 == 0) ? outerR : outerR - 1.4f;
        const float px = cx + r * std::cos (a);
        const float py = cy + r * std::sin (a);
        if (i == 0) knurl.startNewSubPath (px, py);
        else knurl.lineTo (px, py);
    }
    knurl.closeSubPath();
    g.setColour (juce::Colour (0xff1a1d23));
    g.fillPath (knurl);
    g.setColour (juce::Colour (0xff090a0d));
    g.strokePath (knurl, juce::PathStrokeType (0.8f));

    // Machined metal cap with top-left specular highlight
    const float capR = outerR * 0.83f;
    juce::ColourGradient capGrad (
        isBig ? juce::Colour (0xff68707e) : juce::Colour (0xff565e6b),
        cx - capR * 0.6f, cy - capR * 0.6f,
        juce::Colour (0xff111318),
        cx + capR * 0.65f, cy + capR * 0.65f, true);
    g.setGradientFill (capGrad);
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colour (0x33ffffff));
    g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 0.9f);

    // Engraved white indicator line
    juce::Path pointer;
    pointer.addRectangle (-1.1f, -capR + 1.5f, 2.2f, capR * 0.72f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (juce::Colour (0xfff8fafc));
    g.fillPath (pointer);
}

static constexpr const char* kParamIds[16] = {
    "preDelayMs", "decay", "mix",
    "size", "shape", "spread", "bassMult", "crossoverHz",
    "highCutHz", "diffusion", "spin", "wander", "width", "ducking",
    "eqLowCutHz", "eqHighCutHz"
};

static constexpr const char* kParamLabels[16] = {
    "PRE-DELAY", "DECAY", "MIX",
    "SIZE", "SHAPE", "SPREAD", "BASS MULT", "CROSSOVER",
    "DAMP CUT", "DIFFUSION", "SPIN", "WANDER", "WIDTH", "DUCKING",
    "LOW CUT", "HIGH CUT"
};

static constexpr const char* kAlgoNames[7] = {
    "RANDOM HALL", "CONCERT HALL", "HALL", "CHAMBER", "ROOM", "VINTAGE PLATE", "AMBIENCE"
};

HaldenReverbAudioProcessorEditor::HaldenReverbAudioProcessorEditor (HaldenReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (int i = 0; i < 7; ++i)
    {
        auto& btn = algoButtons[static_cast<size_t> (i)];
        btn.setButtonText (kAlgoNames[i]);
        btn.setClickingTogglesState (false);
        btn.onClick = [this, i]()
        {
            if (auto* param = audioProcessor.apvts.getParameter ("algorithm"))
                param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (i)));
        };
        addAndMakeVisible (btn);
    }

    eqToggle.setClickingTogglesState (true);
    addAndMakeVisible (eqToggle);
    eqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "eqEnabled", eqToggle);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        const bool isBig = i < 3;
        k.slider.setName (kParamIds[i]);
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, isBig ? 74 : 58, 16);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff090b0e));
        k.slider.setColour (juce::Slider::textBoxTextColourId, isBig ? juce::Colour (0xfffbbf24) : juce::Colour (0xffe5be73));
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff000000));
        addAndMakeVisible (k.slider);

        k.label.setText (kParamLabels[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (isBig ? 12.0f : 10.5f, juce::Font::bold));
        k.label.setColour (juce::Label::textColourId, isBig ? juce::Colour (0xffdde2ea) : juce::Colour (0xff9fa6b2));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, kParamIds[i], k.slider);
    }

    setSize (1000, 600);
    startTimerHz (60);
}

HaldenReverbAudioProcessorEditor::~HaldenReverbAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HaldenReverbAudioProcessorEditor::timerCallback()
{
    animPhase += 0.016f;
    const float rawIn = audioProcessor.inEnergy.load (std::memory_order_relaxed);
    const float rawOut = audioProcessor.tailEnergy.load (std::memory_order_relaxed);
    inVuLevel += (rawIn > inVuLevel ? 0.36f : 0.09f) * (rawIn - inVuLevel);
    outVuLevel += (rawOut > outVuLevel ? 0.32f : 0.07f) * (rawOut - outVuLevel);
    smoothedTail += (rawOut > smoothedTail ? 0.35f : 0.08f) * (rawOut - smoothedTail);

    const bool eqOn = audioProcessor.apvts.getRawParameterValue ("eqEnabled")->load() >= 0.5f;
    eqToggle.setButtonText (eqOn ? "EQ ACTIVE" : "EQ BYPASS");

    repaint();
}

void HaldenReverbAudioProcessorEditor::drawVuMeter (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                    const juce::String& label, float level)
{
    g.setColour (juce::Colour (0xff090a0d));
    g.fillRect (bounds);
    g.setColour (juce::Colours::black);
    g.drawRect (bounds, 1.5f);

    const auto dial = bounds.reduced (4.0f);
    juce::ColourGradient glow (juce::Colour (0xff9c611b), dial.getCentreX(), dial.getBottom(),
                               juce::Colour (0xff261605), dial.getX(), dial.getY(), true);
    g.setGradientFill (glow);
    g.fillRect (dial);

    const float px = dial.getCentreX();
    const float py = dial.getBottom() + 8.0f;
    const float r = dial.getHeight() * 0.82f;

    juce::Path arcNorm, arcRed;
    arcNorm.addCentredArc (px, py, r, r, 0.0f, -0.78f, 0.28f, true);
    arcRed.addCentredArc (px, py, r, r, 0.0f, 0.28f, 0.68f, true);
    g.setColour (juce::Colour (0xff1a1004));
    g.strokePath (arcNorm, juce::PathStrokeType (1.5f));
    g.setColour (juce::Colour (0xff991b1b));
    g.strokePath (arcRed, juce::PathStrokeType (3.0f));

    g.setColour (juce::Colour (0xff1b1105));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("VU  " + label, dial.toNearestInt(), juce::Justification::centredBottom);

    const float angle = -0.78f + juce::jlimit (0.0f, 1.0f, level * 1.15f) * 1.46f;
    g.setColour (juce::Colour (0xff120b03));
    g.drawLine (px, py, px + r * 1.02f * std::sin (angle), py - r * 1.02f * std::cos (angle), 1.7f);
}

void HaldenReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Rack ears (32px left & right)
    g.setColour (juce::Colour (0xff111317));
    g.fillAll();

    // Brushed dark anodized aluminium faceplate
    const juce::Rectangle<float> plate (32.0f, 0.0f, 936.0f, 600.0f);
    juce::ColourGradient metalGrad (juce::Colour (0xff21242a), 0.0f, 0.0f,
                                    juce::Colour (0xff111317), 0.0f, 600.0f, false);
    g.setGradientFill (metalGrad);
    g.fillRect (plate);
    g.setColour (juce::Colours::black);
    g.drawRect (plate, 1.0f);

    // 4 Corner Screws
    const float sx[4] = { 44.0f, 956.0f, 44.0f, 956.0f };
    const float sy[4] = { 12.0f, 12.0f, 588.0f, 588.0f };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (juce::Colour (0xff07080a));
        g.fillEllipse (sx[i] - 6.0f, sy[i] - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colour (0xff4b5563));
        g.fillEllipse (sx[i] - 4.5f, sy[i] - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xff090a0d));
        g.drawLine (sx[i] - 3.0f, sy[i] - 2.0f, sx[i] + 3.0f, sy[i] + 2.0f, 1.4f);
    }

    // Left Logo Block
    g.setColour (juce::Colour (0xffe6e9ef));
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (JucePlugin_Name, 60, 22, 174, 24, juce::Justification::left);
    g.setColour (juce::Colour (0xff828995));
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("STUDIO DIGITAL REVERBERATOR", 60, 46, 174, 14, juce::Justification::left);

    // Active Algorithm LEDs beside buttons
    const int activeAlgo = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("algorithm")->load());
    for (int i = 0; i < 7; ++i)
    {
        const auto b = algoButtons[static_cast<size_t> (i)].getBounds();
        g.setColour (i == activeAlgo ? juce::Colour (0xfffbbf24) : juce::Colour (0xff2a1d08));
        g.fillRect (b.getX() - 10, b.getY() + 10, 5, b.getHeight() - 20);
    }

    // Dark Green/Amber LCD Display with Scanlines
    const juce::Rectangle<float> lcd (250.0f, 16.0f, 384.0f, 268.0f);
    g.setColour (juce::Colour (0xff06120b));
    g.fillRect (lcd);
    g.setColour (juce::Colours::black);
    g.drawRect (lcd, 2.0f);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (lcd.reduced (3.0f).toNearestInt());

        const float decaySec = audioProcessor.apvts.getRawParameterValue ("decay")->load();
        const float bassMult = audioProcessor.apvts.getRawParameterValue ("bassMult")->load();
        const float preMs = audioProcessor.apvts.getRawParameterValue ("preDelayMs")->load();
        const float shape = audioProcessor.apvts.getRawParameterValue ("shape")->load();
        const float spread = audioProcessor.apvts.getRawParameterValue ("spread")->load();
        const bool eqOn = audioProcessor.apvts.getRawParameterValue ("eqEnabled")->load() >= 0.5f;
        const float eqLowHz = audioProcessor.apvts.getRawParameterValue ("eqLowCutHz")->load();
        const float eqHighHz = audioProcessor.apvts.getRawParameterValue ("eqHighCutHz")->load();

        const float gx = lcd.getX() + 12.0f;
        const float gw = lcd.getWidth() - 24.0f;
        const float baseY = lcd.getBottom() - 18.0f;
        const float usableH = lcd.getHeight() - 52.0f;

        // Spectrum Bars
        const float barW = gw / static_cast<float> (HaldenReverbAudioProcessor::kSpectrumBands);
        for (int b = 0; b < HaldenReverbAudioProcessor::kSpectrumBands; ++b)
        {
            const float mag = audioProcessor.spectrumBins[static_cast<size_t> (b)].load (std::memory_order_relaxed);
            if (mag <= 0.01f) continue;
            const float bh = mag * usableH * 0.9f;
            g.setColour (eqOn ? juce::Colour (0x55a3e635) : juce::Colour (0x44f59e0b));
            g.fillRect (gx + static_cast<float> (b) * barW + 1.0f, baseY - bh, std::max (2.0f, barW - 3.0f), bh);
        }

        // Decay Envelope
        const float preX = gx + std::min (80.0f, (preMs / 250.0f) * 70.0f);
        const float availW = (gx + gw) - preX;
        const float maxSec = std::max (4.5f, decaySec * std::max (1.0f, bassMult) * 1.12f);
        const float bloomW = (0.04f + shape * 0.15f + spread * 0.08f) * availW;
        juce::Path envP;
        for (int i = 0; i <= 72; ++i)
        {
            const float tNorm = static_cast<float> (i) / 72.0f;
            const float px = preX + tNorm * availW;
            const float tSec = tNorm * maxSec;
            const float dist = px - preX;
            const float attack = dist < bloomW ? std::pow (dist / std::max (1.0f, bloomW), 0.7f + shape * 1.3f) : 1.0f;
            const float dec = std::pow (10.0f, (-3.0f * tSec) / std::max (0.15f, decaySec));
            const float py = baseY - juce::jlimit (0.0f, 1.0f, attack * dec) * usableH * 0.88f;
            if (i == 0) envP.startNewSubPath (px, py);
            else envP.lineTo (px, py);
        }
        g.setColour (juce::Colour (0xfffbbf24));
        g.strokePath (envP, juce::PathStrokeType (1.9f));

        // Post-EQ Curve
        juce::Path eqPath;
        for (int i = 0; i <= 80; ++i)
        {
            const float normX = static_cast<float> (i) / 80.0f;
            const float freq = 20.0f * std::pow (1000.0f, normX);
            const float rLow = eqLowHz / std::max (20.0f, freq);
            const float rHigh = freq / std::max (200.0f, eqHighHz);
            const float mag = (1.0f / std::sqrt (1.0f + std::pow (rLow, 4.0f)))
                            * (1.0f / std::sqrt (1.0f + std::pow (rHigh, 4.0f)));
            const float px = gx + normX * gw;
            const float py = baseY - mag * usableH * 0.84f;
            if (i == 0) eqPath.startNewSubPath (px, py);
            else eqPath.lineTo (px, py);
        }
        g.setColour (eqOn ? juce::Colour (0xff84cc16) : juce::Colour (0x5584cc16));
        g.strokePath (eqPath, juce::PathStrokeType (eqOn ? 2.0f : 1.2f));

        // LCD Scanlines
        g.setColour (juce::Colour (0x38000000));
        for (float syLine = lcd.getY(); syLine < lcd.getBottom(); syLine += 3.0f)
            g.drawHorizontalLine (static_cast<int> (syLine), lcd.getX(), lcd.getRight());
    }

    // Dual Analog VU Meters below LCD
    drawVuMeter (g, { 250.0f, 294.0f, 187.0f, 104.0f }, "INPUT", inVuLevel);
    drawVuMeter (g, { 447.0f, 294.0f, 187.0f, 104.0f }, "OUTPUT", outVuLevel);

    // Bottom Divider Line
    g.setColour (juce::Colour (0x1affffff));
    g.drawLine (52.0f, 414.0f, 948.0f, 414.0f, 1.0f);
}

void HaldenReverbAudioProcessorEditor::resized()
{
    // Left Bay: 7 Algorithm LED Buttons
    for (int i = 0; i < 7; ++i)
        algoButtons[static_cast<size_t> (i)].setBounds (70, 72 + i * 44, 166, 38);

    // Right Bay: 3 Big Knobs (PRE-DELAY, DECAY, MIX)
    for (int i = 0; i < 3; ++i)
    {
        const int x = 648 + i * 100;
        knobs[static_cast<size_t> (i)].label.setBounds (x, 42, 96, 16);
        knobs[static_cast<size_t> (i)].slider.setBounds (x + 2, 62, 92, 118);
    }

    // Post-EQ Toggle
    eqToggle.setBounds (830, 320, 108, 52);

    // Bottom Row: 13 Small Knobs
    const int startX = 52;
    const int cellW = 69;
    for (int idx = 3; idx < 16; ++idx)
    {
        const int col = idx - 3;
        const int x = startX + col * cellW;
        knobs[static_cast<size_t> (idx)].label.setBounds (x, 434, cellW, 14);
        knobs[static_cast<size_t> (idx)].slider.setBounds (x + 5, 452, 58, 88);
    }
}
