#pragma once

#include "PluginProcessor.h"
#include <array>

class HaldenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HaldenLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

class HaldenReverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit HaldenReverbAudioProcessorEditor (HaldenReverbAudioProcessor&);
    ~HaldenReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawVuMeter (juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& label, float level);

    HaldenReverbAudioProcessor& audioProcessor;
    HaldenLookAndFeel lookAndFeel;

    std::array<juce::TextButton, 7> algoButtons;
    juce::TextButton eqToggle { "EQ BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqAttachment;

    struct KnobControl
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // 0..2: Big Knobs (preDelayMs, decay, mix)
    // 3..15: 13 Bottom Row Knobs (size..ducking, eqLowCutHz, eqHighCutHz)
    std::array<KnobControl, 16> knobs;
    float inVuLevel = 0.0f;
    float outVuLevel = 0.0f;
    float smoothedTail = 0.0f;
    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaldenReverbAudioProcessorEditor)
};
