#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

//==============================================================================
class AutoGainProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    AutoGainProcessor();
    ~AutoGainProcessor() override = default;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AutoGain Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    static constexpr const char* paramTargetLevel = "targetLevel";
    static constexpr const char* paramAttack = "attackMs";
    static constexpr const char* paramRelease = "releaseMs";
    static constexpr const char* paramActive = "isActive";
    static constexpr const char* paramPreSource = "preSource";
    static constexpr const char* paramFreeze = "freeze";
    static constexpr const char* paramDryWet = "dryWet";
    static constexpr const char* paramThreshold = "threshold";
    static constexpr const char* paramKnee = "knee";
    static constexpr const char* paramMaxBoost = "maxBoost";
    static constexpr const char* paramMaxCut = "maxCut";
    static constexpr const char* paramLookahead = "lookaheadMs";
    static constexpr const char* paramSidechainHPF = "sidechainHPF";
    static constexpr const char* paramStereoLink = "stereoLink";

    enum PreSource { MainInput = 0, SidechainInput = 1 };

    //==============================================================================
    float getCurrentGainDB() const { return currentGainDBAtomic.load(std::memory_order_relaxed); }
    float getCurrentGainReductionDB() const { return gainReductionDBAtomic.load(std::memory_order_relaxed); }

    void savePreset(const juce::String& name);
    void loadPreset(const juce::String& name);
    juce::StringArray getAvailablePresets() const;

    // Added: Required declaration for preset directory
    juce::File getPresetDirectory() const;

    bool isParameterBeingLearned(const juce::String& paramID) const;
    void startMIDILearn(const juce::String& paramID);
    void stopMIDILearn();
    const juce::String& getCurrentMIDILearnParam() const { return currentlyLearningParam; }

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    class EnvelopeFollower
    {
    public:
        EnvelopeFollower() = default;
        void reset();
        void setSampleRate(double sr) { sampleRate = sr; }
        void setTimes(float attackMs, float releaseMs);
        void processBlock(const float* input, float* output, int numSamples);

    private:
        double sampleRate = 44100.0;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float state = 0.0f;
    };

    class GainRamp
    {
    public:
        GainRamp() = default;
        void reset();
        void setSampleRate(double sr) { sampleRate = sr; }
        void setTimes(float attackMs, float releaseMs);
        float process(float target, int numSamples);

    private:
        double sampleRate = 44100.0;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float currentValue = 1.0f;
    };

    class HighPassFilter
    {
    public:
        HighPassFilter() = default;
        void setCutoffHz(float hz, double sr);
        void processBlock(const float* input, float* output, int numSamples);

    private:
        double sampleRate = 44100.0;
        float alpha = 0.0f;
        float xPrev = 0.0f, yPrev = 0.0f;
    };

    //==============================================================================
    float computeRMSdB(const juce::AudioBuffer<float>& buffer, int numSamples, bool linked);
    float calculateGainAdjustmentDB(float inputLevelDB, float targetDB, float thresholdDB, float kneeDB);

    //==============================================================================
    std::atomic<float>* targetLevelParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* activeParam = nullptr;
    std::atomic<float>* preSourceParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* dryWetParam = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* maxBoostParam = nullptr;
    std::atomic<float>* maxCutParam = nullptr;
    std::atomic<float>* lookaheadParam = nullptr;
    std::atomic<float>* sidechainHPFParam = nullptr;
    std::atomic<float>* stereoLinkParam = nullptr;

    EnvelopeFollower mainEnvelopeFollower;
    EnvelopeFollower sidechainEnvelopeFollower;
    GainRamp gainRamp;
    HighPassFilter sidechainHPF;

    std::vector<float> delayBufferL, delayBufferR;
    int delayWritePos = 0, delayReadPos = 0, delayLength = 0;

    mutable std::atomic<float> currentGainDBAtomic{ 0.0f };
    mutable std::atomic<float> gainReductionDBAtomic{ 0.0f };

    float frozenGainLinear = 1.0f;
    bool latencyChanged = false;
    int currentLatencySamples = 0;

    juce::String currentlyLearningParam;
    mutable juce::CriticalSection learnLock;
    juce::ValueTree presetStore{ "Presets" };

    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoGainProcessor)
};