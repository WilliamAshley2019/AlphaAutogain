#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class AutoGainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AutoGainEditor(AutoGainProcessor&);

    // FIXED: Removed "= default" so it matches the implementation in PluginEditor.cpp
    ~AutoGainEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    AutoGainProcessor& processor;

    juce::Slider targetKnob, attackKnob, releaseKnob, dryWetKnob;
    juce::Slider thresholdKnob, kneeKnob, maxBoostKnob, maxCutKnob;
    juce::Slider lookaheadKnob, sidechainHPFKnob;
    juce::ToggleButton activeButton, freezeButton, stereoLinkButton;
    juce::ComboBox preSourceCombo;

    // Dual meter
    class DualMeter : public juce::Component
    {
    public:
        DualMeter() = default;
        void setValues(float gain, float reduction);
        void paint(juce::Graphics&) override;

        enum ColourIds
        {
            backgroundColourId = 0x2000000,
            gainForegroundColourId,
            reductionForegroundColourId,
            unityMarkerColourId,
            textColourId
        };

    private:
        float gainVal = 0.5f, redVal = 0.5f;
    };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> tAtt, aAtt, rAtt, dAtt, thAtt, kAtt, mbAtt, mcAtt, lAtt, hAtt;
    std::unique_ptr<ButtonAtt> actAtt, frzAtt, slAtt;
    std::unique_ptr<ComboAtt>  psAtt;

    juce::Label targetLabel, attackLabel, releaseLabel, dryWetLabel;
    juce::Label thresholdLabel, kneeLabel, maxBoostLabel, maxCutLabel;
    juce::Label lookaheadLabel, hpfLabel;

    // CRITICAL FIX: Added missing member declaration that caused C2065 errors
    DualMeter mainMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoGainEditor)
};