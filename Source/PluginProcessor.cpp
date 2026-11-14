#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

HaldenReverbAudioProcessor::HaldenReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& b : spectrumBins)
        b.store (0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout HaldenReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "algorithm", "Algorithm",
        juce::StringArray { "Random Hall", "Concert Hall", "Hall", "Chamber", "Room", "Vintage Plate", "Ambience" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("preDelayMs", "Pre-Delay", juce::NormalisableRange<float> (0.0f, 250.0f, 0.5f), 28.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("size", "Size", juce::NormalisableRange<float> (0.25f, 1.50f, 0.01f), 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("decay", "Decay", juce::NormalisableRange<float> (0.20f, 15.0f, 0.05f, 0.55f), 3.10f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("bassMult", "Bass Mult", juce::NormalisableRange<float> (0.40f, 2.50f, 0.01f), 1.30f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("crossoverHz", "Crossover", juce::NormalisableRange<float> (80.0f, 1600.0f, 1.0f, 0.5f), 360.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("highCutHz", "High Cut", juce::NormalisableRange<float> (1200.0f, 18000.0f, 10.0f, 0.5f), 6800.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("diffusion", "Diffusion", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.82f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("shape", "Shape", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.42f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spread", "Spread", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("spin", "Spin", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.52f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("wander", "Wander", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.48f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width", "Width", juce::NormalisableRange<float> (0.0f, 1.50f, 0.01f), 1.05f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ducking", "Ducking", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.00f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  ("eqEnabled", "Post EQ", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqLowCutHz", "EQ Low Cut", juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.45f), 500.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("eqHighCutHz", "EQ High Cut", juce::NormalisableRange<float> (1000.0f, 20000.0f, 10.0f, 0.45f), 6000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.36f));

    return { params.begin(), params.end() };
}

void HaldenReverbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    srScale = static_cast<float> (sampleRate / 29761.0);

    preDelayL.resize (static_cast<int> (std::ceil (sampleRate * 0.35)));
    preDelayR.resize (static_cast<int> (std::ceil (sampleRate * 0.35)));
    earlyDelay.resize (static_cast<int> (std::ceil (sampleRate * 0.25)));

    inAp1.resize (static_cast<int> (std::ceil (1200.0f * srScale)));
    inAp2.resize (static_cast<int> (std::ceil (1200.0f * srScale)));
    inAp3.resize (static_cast<int> (std::ceil (2000.0f * srScale)));
    inAp4.resize (static_cast<int> (std::ceil (2000.0f * srScale)));

    modApL.resize (static_cast<int> (std::ceil (3500.0f * srScale)));
    modApR.resize (static_cast<int> (std::ceil (3500.0f * srScale)));
    delayL1.resize (static_cast<int> (std::ceil (10000.0f * srScale)));
    delayR1.resize (static_cast<int> (std::ceil (10000.0f * srScale)));
    apL2.resize (static_cast<int> (std::ceil (6000.0f * srScale)));
    apR2.resize (static_cast<int> (std::ceil (6000.0f * srScale)));
    delayL2.resize (static_cast<int> (std::ceil (9000.0f * srScale)));
    delayR2.resize (static_cast<int> (std::ceil (9000.0f * srScale)));

    releaseResources();
}

void HaldenReverbAudioProcessor::releaseResources()
{
    preDelayL.reset();
    preDelayR.reset();
    earlyDelay.reset();
    inAp1.reset(); inAp2.reset(); inAp3.reset(); inAp4.reset();
    modApL.reset(); modApR.reset();
    delayL1.reset(); delayR1.reset();
    apL2.reset(); apR2.reset();
    delayL2.reset(); delayR2.reset();
    eqHpfL.reset(); eqHpfR.reset();
    eqLpfL.reset(); eqLpfR.reset();
    tankL = tankR = xoverLpL = xoverLpR = dampLpL = dampLpR = inHpL = inHpR = dryEnv = 0.0f;
    fftFifo.fill (0.0f);
    fftFifoIndex = 0;
    inEnergy.store (0.0f, std::memory_order_relaxed);
    tailEnergy.store (0.0f, std::memory_order_relaxed);
    duckGainReduction.store (1.0f, std::memory_order_relaxed);
}

bool HaldenReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}

void HaldenReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : outL;

    struct AlgoProfile { float tankScale, erWeight, lateWeight, bloomBias, modBias, highBias; };
    static constexpr std::array<AlgoProfile, 7> profiles {{
        { 1.00f, 0.32f, 0.85f, 0.35f, 1.15f, 1.00f }, // Random Hall
        { 1.18f, 0.28f, 0.90f, 0.55f, 0.95f, 0.95f }, // Concert Hall
        { 0.92f, 0.36f, 0.84f, 0.25f, 0.85f, 1.00f }, // Hall
        { 0.72f, 0.44f, 0.82f, 0.15f, 0.75f, 1.05f }, // Chamber
        { 0.54f, 0.58f, 0.72f, 0.08f, 0.60f, 0.92f }, // Room
        { 0.68f, 0.18f, 0.94f, 0.02f, 0.80f, 0.82f }, // Vintage Plate
        { 0.42f, 0.85f, 0.28f, 0.05f, 0.40f, 1.10f }  // Ambience
    }};

    const int algoIdx = juce::jlimit (0, 6, static_cast<int> (apvts.getRawParameterValue ("algorithm")->load()));
    const auto& prof = profiles[static_cast<size_t> (algoIdx)];

    const float sr = static_cast<float> (currentSampleRate);
    const float preDelayMs = apvts.getRawParameterValue ("preDelayMs")->load();
    const float sizeParam = apvts.getRawParameterValue ("size")->load() * prof.tankScale;
    const float decaySec = apvts.getRawParameterValue ("decay")->load();
    const float bassMult = apvts.getRawParameterValue ("bassMult")->load();
    const float crossoverHz = apvts.getRawParameterValue ("crossoverHz")->load();
    const float highCutHz = std::min (sr * 0.45f, apvts.getRawParameterValue ("highCutHz")->load() * prof.highBias);
    const float diffusion = apvts.getRawParameterValue ("diffusion")->load();
    const float shape = apvts.getRawParameterValue ("shape")->load();
    const float spread = apvts.getRawParameterValue ("spread")->load();
    const float spin = apvts.getRawParameterValue ("spin")->load();
    const float wander = apvts.getRawParameterValue ("wander")->load();
    const float width = apvts.getRawParameterValue ("width")->load();
    const float ducking = apvts.getRawParameterValue ("ducking")->load();
    const bool eqEnabled = apvts.getRawParameterValue ("eqEnabled")->load() >= 0.5f;
    const float eqLowCutHz = apvts.getRawParameterValue ("eqLowCutHz")->load();
    const float eqHighCutHz = apvts.getRawParameterValue ("eqHighCutHz")->load();
    const float mix = apvts.getRawParameterValue ("mix")->load();

    if (eqEnabled)
    {
        eqHpfL.setHighPass (eqLowCutHz, sr);
        eqHpfR.setHighPass (eqLowCutHz, sr);
        eqLpfL.setLowPass (eqHighCutHz, sr);
        eqLpfR.setLowPass (eqHighCutHz, sr);
    }

    const float preSamples = std::max (1.0f, (preDelayMs * 0.001f) * sr);
    const float inDiff1 = 0.25f + diffusion * 0.50f;
    const float inDiff2 = 0.18f + diffusion * 0.45f;
    const float tankDiff1 = 0.35f + diffusion * 0.35f;
    const float tankDiff2 = 0.25f + diffusion * 0.25f;

    const float baseLoopSamples = (4453.0f + 3720.0f + 672.0f + 1800.0f) * srScale * sizeParam;
    const float loopSec = std::max (0.04f, baseLoopSamples / sr);
    const float gMid = (algoIdx == 6)
        ? std::min (0.45f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec * 0.35f)))
        : std::min (0.988f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec)));
    const float gLow = std::min (0.993f, std::pow (10.0f, (-3.0f * loopSec) / std::max (0.15f, decaySec * bassMult)));

    const float twoPi = 6.28318530718f;
    const float xoverAlpha = 1.0f - std::exp ((-twoPi * crossoverHz) / sr);
    const float highAlpha = 1.0f - std::exp ((-twoPi * highCutHz) / sr);
    const float hpAlpha = 1.0f - std::exp ((-twoPi * 35.0f) / sr);
    const float envAtk = 1.0f - std::exp (-1.0f / (0.004f * sr));
    const float envRel = 1.0f - std::exp (-1.0f / (0.18f * sr));

    const float spinInc = (twoPi * (0.15f + spin * 1.35f)) / sr;
    const float wanderInc = (twoPi * (0.04f + wander * 0.38f)) / sr;
    const float modDepth = (2.5f + spin * 14.0f + wander * 16.0f) * srScale * prof.modBias;

    const float erSpanMs = (22.0f + spread * 95.0f) * (0.6f + sizeParam * 0.4f);
    const float bloomShape = std::min (1.0f, shape * 0.75f + prof.bloomBias * 0.45f);
    const float erMixWeight = prof.erWeight * (1.15f - bloomShape * 0.45f);
    const float lateMixWeight = prof.lateWeight;

    static constexpr std::array<float, 8> erTapRatios { 0.07f, 0.13f, 0.21f, 0.31f, 0.43f, 0.57f, 0.74f, 0.92f };
    const float tankClampSize = juce::jlimit (0.45f, 1.40f, sizeParam);

    float drySqSum = 0.0f;
    float wetSqSum = 0.0f;
    float minDuckGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = inL[i];
        const float dryR = inR[i];
        const float monoDry = 0.5f * (dryL + dryR);
        drySqSum += monoDry * monoDry;

        const float dryLvl = std::max (std::abs (dryL), std::abs (dryR));
        dryEnv += (dryLvl > dryEnv ? envAtk : envRel) * (dryLvl - dryEnv);
        const float duckGain = ducking > 0.001f
            ? std::max (0.06f, 1.0f / (1.0f + dryEnv * ducking * 22.0f))
            : 1.0f;
        if (duckGain < minDuckGain) minDuckGain = duckGain;

        inHpL += hpAlpha * (dryL - inHpL);
        inHpR += hpAlpha * (dryR - inHpR);
        preDelayL.write (dryL - inHpL);
        preDelayR.write (dryR - inHpR);

        const float preL = preDelayL.readFrac (preSamples);
        const float preR = preDelayR.readFrac (preSamples);
        const float monoIn = 0.5f * (preL + preR);

        float diff = inAp1.process (monoIn, 142.0f * srScale, inDiff1);
        diff = inAp2.process (diff, 107.0f * srScale, inDiff1);
        diff = inAp3.process (diff, 379.0f * srScale, inDiff2);
        diff = inAp4.process (diff, 277.0f * srScale, inDiff2);

        earlyDelay.write (0.65f * diff + 0.35f * monoIn);
        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < 8; ++t)
        {
            const float tapMs = 4.0f + erTapRatios[static_cast<size_t> (t)] * erSpanMs;
            const float tapVal = earlyDelay.readFrac ((tapMs * 0.001f) * sr);
            const float pos = static_cast<float> (t) / 7.0f;
            const float envGain = (1.0f - bloomShape) * (1.0f - pos * 0.55f)
                                + bloomShape * (0.25f + pos * 0.75f);
            const float sign = (t & 1) == 0 ? 1.0f : -1.0f;
            if ((t & 1) == 0)
            {
                erL += tapVal * envGain * 0.22f;
                erR += tapVal * envGain * 0.14f * sign;
            }
            else
            {
                erR += tapVal * envGain * 0.22f;
                erL += tapVal * envGain * 0.14f * sign;
            }
        }

        spinPhase += spinInc;
        if (spinPhase > twoPi) spinPhase -= twoPi;
        wanderPhase += wanderInc;
        if (wanderPhase > twoPi) wanderPhase -= twoPi;

        const float lfoL = std::sin (spinPhase) * 0.6f + std::sin (wanderPhase) * 0.4f;
        const float lfoR = std::cos (spinPhase) * 0.6f + std::cos (wanderPhase * 0.87f) * 0.4f;

        const float mApL = modApL.process (diff + tankR, 672.0f * srScale * tankClampSize + lfoL * modDepth, tankDiff1);
        delayL1.write (mApL);
        const float dL1Out = delayL1.readFrac (4453.0f * srScale * sizeParam);

        xoverLpL += xoverAlpha * (dL1Out - xoverLpL);
        dampLpL += highAlpha * ((dL1Out - xoverLpL) - dampLpL);
        const float dampedL = xoverLpL * gLow + dampLpL * gMid;

        delayL2.write (apL2.process (dampedL, 1800.0f * srScale * sizeParam, -tankDiff2));
        const float nextTankL = delayL2.readFrac (3720.0f * srScale * sizeParam);

        const float mApR = modApR.process (diff + tankL, 908.0f * srScale * tankClampSize + lfoR * modDepth, tankDiff1);
        delayR1.write (mApR);
        const float dR1Out = delayR1.readFrac (4217.0f * srScale * sizeParam);

        xoverLpR += xoverAlpha * (dR1Out - xoverLpR);
        dampLpR += highAlpha * ((dR1Out - xoverLpR) - dampLpR);
        const float dampedR = xoverLpR * gLow + dampLpR * gMid;

        delayR2.write (apR2.process (dampedR, 2656.0f * srScale * sizeParam, -tankDiff2));
        const float nextTankR = delayR2.readFrac (3163.0f * srScale * sizeParam);

        tankL = std::abs (nextTankL) < 1.0e-18f ? 0.0f : nextTankL;
        tankR = std::abs (nextTankR) < 1.0e-18f ? 0.0f : nextTankR;

        float wetL =
            0.6f * delayR1.readFrac (266.0f * srScale * sizeParam) +
            0.6f * delayR1.readFrac (2974.0f * srScale * sizeParam) -
            0.5f * apR2.delay.readFrac (1235.0f * srScale * sizeParam) +
            0.5f * delayR2.readFrac (1996.0f * srScale * sizeParam) -
            0.5f * delayL1.readFrac (1990.0f * srScale * sizeParam) -
            0.4f * apL2.delay.readFrac (187.0f * srScale * sizeParam) -
            0.4f * delayL2.readFrac (1066.0f * srScale * sizeParam);

        float wetR =
            0.6f * delayL1.readFrac (353.0f * srScale * sizeParam) +
            0.6f * delayL1.readFrac (3627.0f * srScale * sizeParam) -
            0.5f * apL2.delay.readFrac (1228.0f * srScale * sizeParam) +
            0.5f * delayL2.readFrac (2673.0f * srScale * sizeParam) -
            0.5f * delayR1.readFrac (2111.0f * srScale * sizeParam) -
            0.4f * apR2.delay.readFrac (335.0f * srScale * sizeParam) -
            0.4f * delayR2.readFrac (121.0f * srScale * sizeParam);

        wetL = (erL * erMixWeight + wetL * 0.42f * lateMixWeight) * duckGain;
        wetR = (erR * erMixWeight + wetR * 0.42f * lateMixWeight) * duckGain;

        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * width;
        float finalWetL = mid + side;
        float finalWetR = mid - side;

        if (eqEnabled)
        {
            finalWetL = eqLpfL.process (eqHpfL.process (finalWetL));
            finalWetR = eqLpfR.process (eqHpfR.process (finalWetR));
        }

        const float monoWet = 0.5f * (finalWetL + finalWetR);
        wetSqSum += monoWet * monoWet;

        // Feed FFT spectrum analyzer FIFO
        fftFifo[static_cast<size_t> (fftFifoIndex++)] = monoWet;
        if (fftFifoIndex >= kFftSize)
        {
            fftFifoIndex = 0;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int k = 0; k < kFftSize; ++k)
            {
                const float win = 0.5f * (1.0f - std::cos (twoPi * static_cast<float> (k) / static_cast<float> (kFftSize - 1)));
                fftData[static_cast<size_t> (k)] = fftFifo[static_cast<size_t> (k)] * win;
            }
            forwardFft.performFrequencyOnlyForwardTransform (fftData.data());
            const float nyquist = sr * 0.5f;
            for (int b = 0; b < kSpectrumBands; ++b)
            {
                const float f0 = 20.0f * std::pow (1000.0f, static_cast<float> (b) / static_cast<float> (kSpectrumBands));
                const float f1 = 20.0f * std::pow (1000.0f, static_cast<float> (b + 1) / static_cast<float> (kSpectrumBands));
                const int b0 = juce::jlimit (1, kFftSize / 2 - 1, static_cast<int> ((f0 / nyquist) * (kFftSize / 2)));
                const int b1 = juce::jlimit (b0, kFftSize / 2 - 1, static_cast<int> ((f1 / nyquist) * (kFftSize / 2)));
                float pk = 0.0f;
                for (int bin = b0; bin <= b1; ++bin)
                    pk = std::max (pk, fftData[static_cast<size_t> (bin)]);
                const float dbNorm = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (pk / static_cast<float> (kFftSize * 0.25f), -72.0f) + 72.0f) / 72.0f);
                const float prev = spectrumBins[static_cast<size_t> (b)].load (std::memory_order_relaxed);
                spectrumBins[static_cast<size_t> (b)].store (prev * 0.65f + dbNorm * 0.35f, std::memory_order_relaxed);
            }
        }

        outL[i] = dryL * (1.0f - mix * 0.65f) + finalWetL * mix;
        if (buffer.getNumChannels() > 1)
            outR[i] = dryR * (1.0f - mix * 0.65f) + finalWetR * mix;
    }

    const float inRms = juce::jlimit (0.0f, 1.0f, std::sqrt (drySqSum / static_cast<float> (numSamples)) * 3.5f);
    const float rms = juce::jlimit (0.0f, 1.0f, std::sqrt (wetSqSum / static_cast<float> (numSamples)) * 3.5f);
    inEnergy.store (inRms, std::memory_order_relaxed);
    tailEnergy.store (rms, std::memory_order_relaxed);
    duckGainReduction.store (minDuckGain, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* HaldenReverbAudioProcessor::createEditor()
{
    return new HaldenReverbAudioProcessorEditor (*this);
}

void HaldenReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HaldenReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HaldenReverbAudioProcessor();
}
