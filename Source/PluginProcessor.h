#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace haldendsp
{
class DelayLine
{
public:
    void resize (int maxSamples)
    {
        buffer.assign (static_cast<size_t> (std::max (256, maxSamples)), 0.0f);
        writeIdx = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

    inline void write (float sample)
    {
        buffer[static_cast<size_t> (writeIdx)] = sample;
        writeIdx = (writeIdx + 1) % static_cast<int> (buffer.size());
    }

    inline float readFrac (float delaySamples) const
    {
        const int sz = static_cast<int> (buffer.size());
        const float clamped = juce::jlimit (1.0f, static_cast<float> (sz - 3), delaySamples);
        const int intPart = static_cast<int> (clamped);
        const float frac = clamped - static_cast<float> (intPart);
        int idx0 = writeIdx - intPart;
        if (idx0 < 0) idx0 += sz;
        int idx1 = idx0 - 1;
        if (idx1 < 0) idx1 += sz;
        return buffer[static_cast<size_t> (idx0)] * (1.0f - frac)
             + buffer[static_cast<size_t> (idx1)] * frac;
    }

    DelayLine() = default;
    std::vector<float> buffer;
    int writeIdx = 0;
};

class AllpassFilter
{
public:
    void resize (int maxSamples) { delay.resize (maxSamples); }
    void reset() { delay.reset(); }

    inline float process (float input, float delaySamples, float coeff)
    {
        const float delayed = delay.readFrac (delaySamples);
        float v = input - coeff * delayed;
        if (std::abs (v) < 1.0e-18f) v = 0.0f;
        v = juce::jlimit (-4.0f, 4.0f, v);
        delay.write (v);
        return delayed + coeff * v;
    }

    DelayLine delay;
};

class BiquadFilter
{
public:
    void reset() { z1 = z2 = 0.0f; }

    void setHighPass (float freq, float sr)
    {
        const float f = juce::jlimit (20.0f, sr * 0.45f, freq);
        const float w0 = 6.28318530718f * f / sr;
        const float cosw0 = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;
        b0 = ((1.0f + cosw0) * 0.5f) / a0;
        b1 = -(1.0f + cosw0) / a0;
        b2 = ((1.0f + cosw0) * 0.5f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setLowPass (float freq, float sr)
    {
        const float f = juce::jlimit (20.0f, sr * 0.45f, freq);
        const float w0 = 6.28318530718f * f / sr;
        const float cosw0 = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;
        b0 = ((1.0f - cosw0) * 0.5f) / a0;
        b1 = (1.0f - cosw0) / a0;
        b2 = ((1.0f - cosw0) * 0.5f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        if (std::abs (z1) < 1.0e-18f) z1 = 0.0f;
        if (std::abs (z2) < 1.0e-18f) z2 = 0.0f;
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};
} // namespace haldendsp

class HaldenReverbAudioProcessor : public juce::AudioProcessor
{
public:
    HaldenReverbAudioProcessor();
    ~HaldenReverbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Lock-free telemetry & 24-band spectrum analyzer for 60 FPS hardware LCD & VU meters
    static constexpr int kSpectrumBands = 24;
    std::atomic<float> inEnergy { 0.0f };
    std::atomic<float> tailEnergy { 0.0f };
    std::atomic<float> duckGainReduction { 1.0f };
    std::array<std::atomic<float>, kSpectrumBands> spectrumBins {};

private:
    double currentSampleRate = 48000.0;
    float srScale = 1.0f;

    haldendsp::DelayLine preDelayL, preDelayR, earlyDelay;
    haldendsp::AllpassFilter inAp1, inAp2, inAp3, inAp4;
    haldendsp::AllpassFilter modApL, modApR, apL2, apR2;
    haldendsp::DelayLine delayL1, delayR1, delayL2, delayR2;
    haldendsp::BiquadFilter eqHpfL, eqHpfR, eqLpfL, eqLpfR;

    // Simple FFT spectrum tracker for the glass analyzer
    static constexpr int kFftOrder = 10;
    static constexpr int kFftSize = 1 << kFftOrder;
    juce::dsp::FFT forwardFft { kFftOrder };
    std::array<float, kFftSize> fftFifo {};
    std::array<float, kFftSize * 2> fftData {};
    int fftFifoIndex = 0;

    float tankL = 0.0f, tankR = 0.0f;
    float xoverLpL = 0.0f, xoverLpR = 0.0f;
    float dampLpL = 0.0f, dampLpR = 0.0f;
    float inHpL = 0.0f, inHpR = 0.0f;
    float dryEnv = 0.0f;
    float spinPhase = 0.0f, wanderPhase = 1.1f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaldenReverbAudioProcessor)
};
