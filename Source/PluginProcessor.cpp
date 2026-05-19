#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <limits>
#include <cmath>

//==============================================================================
AutoGainProcessor::AutoGainProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Main Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Main Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    targetLevelParam = apvts.getRawParameterValue(paramTargetLevel);
    attackParam = apvts.getRawParameterValue(paramAttack);
    releaseParam = apvts.getRawParameterValue(paramRelease);
    activeParam = apvts.getRawParameterValue(paramActive);
    preSourceParam = apvts.getRawParameterValue(paramPreSource);
    freezeParam = apvts.getRawParameterValue(paramFreeze);
    dryWetParam = apvts.getRawParameterValue(paramDryWet);
    thresholdParam = apvts.getRawParameterValue(paramThreshold);
    kneeParam = apvts.getRawParameterValue(paramKnee);
    maxBoostParam = apvts.getRawParameterValue(paramMaxBoost);
    maxCutParam = apvts.getRawParameterValue(paramMaxCut);
    lookaheadParam = apvts.getRawParameterValue(paramLookahead);
    sidechainHPFParam = apvts.getRawParameterValue(paramSidechainHPF);
    stereoLinkParam = apvts.getRawParameterValue(paramStereoLink);
}

//==============================================================================
bool AutoGainProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.inputBuses[0];
    const auto& mainOut = layouts.outputBuses[0];

    if (mainIn != mainOut) return false;
    const int numCh = mainIn.size();
    if (numCh < 1 || numCh > 2) return false;
    if (layouts.inputBuses.size() > 1 && layouts.inputBuses[1] != mainIn) return false;
    return true;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AutoGainProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto makeRange = [](float min, float max, float) {
        return juce::NormalisableRange<float>(min, max, 0.1f, 1.0f);
        };
    auto makeTimeRange = [](float min, float max, float) {
        return juce::NormalisableRange<float>(min, max, 1.0f, 0.3f);
        };

    layout.add(std::make_unique<juce::AudioParameterFloat>(paramTargetLevel, "Target Level (dB)", makeRange(-60, 0, -18), -18.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramAttack, "Attack (ms)", makeTimeRange(1, 1000, 50), 50.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramRelease, "Release (ms)", makeTimeRange(10, 2000, 200), 200.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(paramActive, "Active", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(paramFreeze, "Freeze", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(paramStereoLink, "Stereo Link", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(paramPreSource, "Pre Source", juce::StringArray{ "Main Input", "Sidechain" }, MainInput));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramDryWet, "Dry/Wet Mix (%)", makeRange(0, 100, 100), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramThreshold, "Threshold (dB)", makeRange(-60, 0, -40), -40.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramKnee, "Knee (dB)", makeRange(0, 24, 6), 6.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramMaxBoost, "Max Boost (dB)", makeRange(0, 24, 12), 12.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramMaxCut, "Max Cut (dB)", makeRange(0, 60, 24), 24.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramLookahead, "Lookahead (ms)", makeRange(0, 50, 0), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(paramSidechainHPF, "Sidechain HPF (Hz)", juce::NormalisableRange<float>(20, 500, 1, 0.5f), 100.0f));

    return layout;
}

//==============================================================================
static inline int advanceCircularIndex(int currentIndex, int increment, size_t bufferSize)
{
    return static_cast<int>(
        (static_cast<size_t>(currentIndex) + static_cast<size_t>(increment)) % bufferSize);
}

//==============================================================================
void AutoGainProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mainEnvelopeFollower.setSampleRate(sampleRate);
    sidechainEnvelopeFollower.setSampleRate(sampleRate);
    gainRamp.setSampleRate(sampleRate);

    mainEnvelopeFollower.reset();
    sidechainEnvelopeFollower.reset();
    gainRamp.reset();

    sidechainHPF.setCutoffHz(sidechainHPFParam->load(), sampleRate);

    const double maxLookaheadSec = 0.05;
    const int maxLookaheadSamples = juce::jlimit(0, 192000,
        static_cast<int>(std::floor(sampleRate * maxLookaheadSec + 0.5)));
    const int totalBufferSize = juce::jlimit(0, 192000,
        maxLookaheadSamples + samplesPerBlock + 64);

    delayBufferL.assign(static_cast<size_t>(totalBufferSize), 0.0f);
    delayBufferR.assign(static_cast<size_t>(totalBufferSize), 0.0f);
    delayLength = 0;
    delayWritePos = 0;
    delayReadPos = 0;

    // CORRECT WAY TO SET LATENCY IN JUCE:
    setLatencySamples(0);

    currentGainDBAtomic.store(0.0f, std::memory_order_relaxed);
    gainReductionDBAtomic.store(0.0f, std::memory_order_relaxed);
    frozenGainLinear = 1.0f;
}

void AutoGainProcessor::releaseResources() {}

//==============================================================================
void AutoGainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    float attackMs = attackParam->load();
    float releaseMs = releaseParam->load();

    mainEnvelopeFollower.setTimes(attackMs, releaseMs);
    sidechainEnvelopeFollower.setTimes(attackMs, releaseMs);
    gainRamp.setTimes(attackMs, releaseMs);
    sidechainHPF.setCutoffHz(sidechainHPFParam->load(), getSampleRate());

    float preLevelDB = -120.0f;
    bool useSidechain = (preSourceParam->load() > 0.5f);

    if (useSidechain)
    {
        auto* sidechainBus = getBus(false, 1);
        if (sidechainBus && sidechainBus->isEnabled())
        {
            // Fixed C4239: removed '&' to avoid binding non-const ref to temporary
            auto scBuf = sidechainBus->getBusBuffer(buffer);
            juce::AudioBuffer<float> filtered(scBuf.getNumChannels(), numSamples);
            for (int ch = 0; ch < juce::jmin(scBuf.getNumChannels(), filtered.getNumChannels()); ++ch)
                filtered.copyFrom(ch, 0, scBuf, ch, 0, numSamples);
            for (int ch = 0; ch < filtered.getNumChannels(); ++ch)
                sidechainHPF.processBlock(filtered.getReadPointer(ch), filtered.getWritePointer(ch), numSamples);
            preLevelDB = computeRMSdB(filtered, numSamples, stereoLinkParam->load() > 0.5f);
        }
    }
    if (preLevelDB < -119.0f)
        preLevelDB = computeRMSdB(buffer, numSamples, stereoLinkParam->load() > 0.5f);

    bool active = activeParam->load() > 0.5f;
    bool freeze = freezeParam->load() > 0.5f;
    float targetGainDB = 0.0f;

    if (active && !freeze)
    {
        targetGainDB = calculateGainAdjustmentDB(preLevelDB, targetLevelParam->load(), thresholdParam->load(), kneeParam->load());
        targetGainDB = juce::jlimit(-maxCutParam->load(), maxBoostParam->load(), targetGainDB);
        frozenGainLinear = juce::Decibels::decibelsToGain(targetGainDB);
    }
    else if (freeze)
    {
        targetGainDB = juce::Decibels::gainToDecibels(frozenGainLinear);
    }
    else
    {
        frozenGainLinear = 1.0f;
        targetGainDB = 0.0f;
    }
    gainReductionDBAtomic.store(targetGainDB, std::memory_order_relaxed);

    const float lookaheadMs = juce::jlimit(0.0f, 50.0f, lookaheadParam->load());
    const double sr = getSampleRate();
    const int newDelaySamples = juce::jlimit(0, static_cast<int>(delayBufferL.size()) / 2,
        static_cast<int>(std::floor(sr * (lookaheadMs / 1000.0) + 0.5)));

    if (newDelaySamples != delayLength)
    {
        delayLength = newDelaySamples;
        const size_t bufSize = delayBufferL.size();
        delayReadPos = static_cast<int>(
            (static_cast<size_t>(delayWritePos) + bufSize - static_cast<size_t>(delayLength)) % bufSize);

        // CORRECT WAY TO UPDATE LATENCY IN JUCE:
        setLatencySamples(delayLength);
        latencyChanged = true;
    }

    if (delayLength > 0 && numChannels >= 2)
    {
        const auto* lIn = buffer.getReadPointer(0);
        const auto* rIn = buffer.getReadPointer(1);
        auto* lOut = buffer.getWritePointer(0);
        auto* rOut = buffer.getWritePointer(1);
        const size_t bufSize = delayBufferL.size();

        for (int i = 0; i < numSamples; ++i)
        {
            delayBufferL[static_cast<size_t>(delayWritePos)] = lIn[i];
            delayBufferR[static_cast<size_t>(delayWritePos)] = rIn[i];
            lOut[i] = delayBufferL[static_cast<size_t>(delayReadPos)];
            rOut[i] = delayBufferR[static_cast<size_t>(delayReadPos)];
            delayWritePos = advanceCircularIndex(delayWritePos, 1, bufSize);
            delayReadPos = advanceCircularIndex(delayReadPos, 1, bufSize);
        }
    }

    float dryWet = dryWetParam->load() / 100.0f;
    float dryGain = 1.0f - dryWet;
    float targetLinear = juce::Decibels::decibelsToGain(targetGainDB);

    for (int i = 0; i < numSamples; ++i)
    {
        float currentGain = gainRamp.process(targetLinear, 1);
        currentGainDBAtomic.store(juce::Decibels::gainToDecibels(currentGain), std::memory_order_relaxed);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float processed = data[i] * currentGain;
            data[i] = data[i] * dryGain + processed * dryWet;
        }
    }
}

//==============================================================================
float AutoGainProcessor::computeRMSdB(const juce::AudioBuffer<float>& buffer, int numSamples, bool linked)
{
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0) return -120.0f;

    EnvelopeFollower& ef = linked ? mainEnvelopeFollower : sidechainEnvelopeFollower;
    juce::AudioBuffer<float> power(1, numSamples);
    power.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        float* dst = power.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            dst[i] += src[i] * src[i];
    }

    if (linked)
    {
        float* data = power.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i) data[i] *= 0.25f;
    }
    else
    {
        float* data = power.getWritePointer(0);
        float invChans = 1.0f / static_cast<float>(numChannels);
        for (int i = 0; i < numSamples; ++i) data[i] *= invChans;
    }

    juce::AudioBuffer<float> env(1, numSamples);
    ef.processBlock(power.getReadPointer(0), env.getWritePointer(0), numSamples);

    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i) sum += env.getSample(0, i);
    float avg = sum / static_cast<float>(numSamples);

    return juce::Decibels::gainToDecibels(std::sqrt(juce::jmax(avg, 1.0e-9f)), -120.0f);
}

float AutoGainProcessor::calculateGainAdjustmentDB(float input, float target, float thresh, float knee)
{
    if (input < thresh - knee * 0.5f) return 0.0f;
    if (input > thresh + knee * 0.5f) return target - input;
    float ratio = (input - (thresh - knee * 0.5f)) / knee;
    return (target - input) * ratio;
}

//==============================================================================
void AutoGainProcessor::EnvelopeFollower::reset() { state = 0.0f; }

void AutoGainProcessor::EnvelopeFollower::setTimes(float attackMs, float releaseMs)
{
    attackCoeff = (attackMs <= 0.001f) ? 0.0f : static_cast<float>(std::exp(-1000.0 / (sampleRate * attackMs)));
    releaseCoeff = (releaseMs <= 0.001f) ? 0.0f : static_cast<float>(std::exp(-1000.0 / (sampleRate * releaseMs)));
    attackCoeff = juce::jlimit(0.0f, 0.999f, attackCoeff);
    releaseCoeff = juce::jlimit(0.0f, 0.999f, releaseCoeff);
}

void AutoGainProcessor::EnvelopeFollower::processBlock(const float* in, float* out, int n)
{
    for (int i = 0; i < n; ++i)
    {
        float c = (in[i] > state) ? attackCoeff : releaseCoeff;
        state = c * state + (1.0f - c) * in[i];
        out[i] = state;
    }
}

//==============================================================================
void AutoGainProcessor::GainRamp::reset() { currentValue = 1.0f; }

void AutoGainProcessor::GainRamp::setTimes(float attackMs, float releaseMs)
{
    attackCoeff = (attackMs <= 0.001f) ? 0.0f : static_cast<float>(std::exp(-1000.0 / (sampleRate * attackMs)));
    releaseCoeff = (releaseMs <= 0.001f) ? 0.0f : static_cast<float>(std::exp(-1000.0 / (sampleRate * releaseMs)));
    attackCoeff = juce::jlimit(0.0f, 0.999f, attackCoeff);
    releaseCoeff = juce::jlimit(0.0f, 0.999f, releaseCoeff);
}

float AutoGainProcessor::GainRamp::process(float target, int n)
{
    float c = (target > currentValue) ? attackCoeff : releaseCoeff;
    for (int i = 0; i < n; ++i)
        currentValue = c * currentValue + (1.0f - c) * target;
    return currentValue;
}

//==============================================================================
void AutoGainProcessor::HighPassFilter::setCutoffHz(float hz, double sr)
{
    sampleRate = sr;
    if (hz <= 20.0f) { alpha = 1.0f; return; }
    double rc = 1.0 / (2.0 * juce::MathConstants<double>::pi * hz);
    double dt = 1.0 / sr;
    alpha = static_cast<float>(dt / (rc + dt));
}

void AutoGainProcessor::HighPassFilter::processBlock(const float* in, float* out, int n)
{
    for (int i = 0; i < n; ++i)
    {
        out[i] = alpha * (in[i] - xPrev + yPrev);
        xPrev = in[i];
        yPrev = out[i];
    }
}

//==============================================================================
juce::File AutoGainProcessor::getPresetDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AutoGain Pro")
        .getChildFile("Presets");
    if (!dir.exists()) dir.createDirectory();
    return dir;
}

void AutoGainProcessor::savePreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    auto file = getPresetDirectory().getChildFile(name + ".agp");
    auto state = apvts.copyState();
    state.setProperty("presetName", name, nullptr);
    state.setProperty("frozenGainLinear", frozenGainLinear, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml) xml->writeTo(file);
}

void AutoGainProcessor::loadPreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    auto file = getPresetDirectory().getChildFile(name + ".agp");
    if (!file.existsAsFile()) return;
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml || !xml->hasTagName("AutoGainState")) return;
    auto state = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(state);
    if (state.hasProperty("frozenGainLinear"))
        frozenGainLinear = static_cast<float>(state.getProperty("frozenGainLinear"));
}

juce::StringArray AutoGainProcessor::getAvailablePresets() const
{
    juce::StringArray presets;
    auto dir = getPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.agp");
        for (auto& f : files) presets.add(f.getFileNameWithoutExtension());
    }
    return presets;
}

bool AutoGainProcessor::isParameterBeingLearned(const juce::String& id) const
{
    juce::ScopedLock sl(learnLock); return currentlyLearningParam == id;
}

void AutoGainProcessor::startMIDILearn(const juce::String& id)
{
    juce::ScopedLock sl(learnLock); currentlyLearningParam = id;
}

void AutoGainProcessor::stopMIDILearn()
{
    juce::ScopedLock sl(learnLock); currentlyLearningParam = {};
}

//==============================================================================
void AutoGainProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    // JUCE 8 requires 3 arguments for setProperty
    state.setProperty("frozenGainLinear", frozenGainLinear, nullptr);
    state.setProperty("currentLatencySamples", currentLatencySamples, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml) copyXmlToBinary(*xml, destData);
}

void AutoGainProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName("AutoGainState")) return;
    auto state = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(state);
    if (state.hasProperty("frozenGainLinear"))
        frozenGainLinear = static_cast<float>(state.getProperty("frozenGainLinear"));
    if (state.hasProperty("currentLatencySamples"))
        currentLatencySamples = static_cast<int>(state.getProperty("currentLatencySamples"));
}

//==============================================================================
juce::AudioProcessorEditor* AutoGainProcessor::createEditor() { return new AutoGainEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutoGainProcessor(); }