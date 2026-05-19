#include "PluginEditor.h"

AutoGainEditor::AutoGainEditor(AutoGainProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    auto setupKnob = [](juce::Slider& k, juce::Label& l) {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        k.setTooltip("");
        l.setJustificationType(juce::Justification::centred);
        // FIXED: JUCE 8 compliant Font constructor (eliminates C4996 warning)
        l.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        };

    setupKnob(targetKnob, targetLabel); targetLabel.setText("Target (dB)", juce::dontSendNotification);
    setupKnob(attackKnob, attackLabel); attackLabel.setText("Attack (ms)", juce::dontSendNotification);
    setupKnob(releaseKnob, releaseLabel); releaseLabel.setText("Release (ms)", juce::dontSendNotification);
    setupKnob(dryWetKnob, dryWetLabel); dryWetLabel.setText("Dry/Wet (%)", juce::dontSendNotification);
    setupKnob(thresholdKnob, thresholdLabel); thresholdLabel.setText("Threshold", juce::dontSendNotification);
    setupKnob(kneeKnob, kneeLabel); kneeLabel.setText("Knee", juce::dontSendNotification);
    setupKnob(maxBoostKnob, maxBoostLabel); maxBoostLabel.setText("Max Boost", juce::dontSendNotification);
    setupKnob(maxCutKnob, maxCutLabel); maxCutLabel.setText("Max Cut", juce::dontSendNotification);
    setupKnob(lookaheadKnob, lookaheadLabel); lookaheadLabel.setText("Lookahead", juce::dontSendNotification);
    setupKnob(sidechainHPFKnob, hpfLabel); hpfLabel.setText("SC HPF", juce::dontSendNotification);

    addAndMakeVisible(targetKnob); addAndMakeVisible(targetLabel);
    addAndMakeVisible(attackKnob); addAndMakeVisible(attackLabel);
    addAndMakeVisible(releaseKnob); addAndMakeVisible(releaseLabel);
    addAndMakeVisible(dryWetKnob); addAndMakeVisible(dryWetLabel);
    addAndMakeVisible(thresholdKnob); addAndMakeVisible(thresholdLabel);
    addAndMakeVisible(kneeKnob); addAndMakeVisible(kneeLabel);
    addAndMakeVisible(maxBoostKnob); addAndMakeVisible(maxBoostLabel);
    addAndMakeVisible(maxCutKnob); addAndMakeVisible(maxCutLabel);
    addAndMakeVisible(lookaheadKnob); addAndMakeVisible(lookaheadLabel);
    addAndMakeVisible(sidechainHPFKnob); addAndMakeVisible(hpfLabel);

    activeButton.setButtonText("ACTIVE");
    activeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::limegreen);
    freezeButton.setButtonText("FREEZE");
    stereoLinkButton.setButtonText("STEREO LINK");
    stereoLinkButton.setToggleState(true, juce::dontSendNotification);

    preSourceCombo.addItem("Main Input", 1);
    preSourceCombo.addItem("Sidechain", 2);

    addAndMakeVisible(activeButton); addAndMakeVisible(freezeButton);
    addAndMakeVisible(stereoLinkButton); addAndMakeVisible(preSourceCombo);

    // Meter setup
    mainMeter.setColour(AutoGainEditor::DualMeter::backgroundColourId, juce::Colours::darkgrey.darker());
    mainMeter.setColour(AutoGainEditor::DualMeter::gainForegroundColourId, juce::Colours::limegreen);
    mainMeter.setColour(AutoGainEditor::DualMeter::reductionForegroundColourId, juce::Colours::orange);
    mainMeter.setColour(AutoGainEditor::DualMeter::unityMarkerColourId, juce::Colours::white.withAlpha(0.3f));
    addAndMakeVisible(mainMeter);

    // APVTS Attachments
    tAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramTargetLevel, targetKnob);
    aAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramAttack, attackKnob);
    rAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramRelease, releaseKnob);
    dAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramDryWet, dryWetKnob);
    thAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramThreshold, thresholdKnob);
    kAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramKnee, kneeKnob);
    mbAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramMaxBoost, maxBoostKnob);
    mcAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramMaxCut, maxCutKnob);
    lAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramLookahead, lookaheadKnob);
    hAtt = std::make_unique<SliderAtt>(processor.apvts, AutoGainProcessor::paramSidechainHPF, sidechainHPFKnob);
    actAtt = std::make_unique<ButtonAtt>(processor.apvts, AutoGainProcessor::paramActive, activeButton);
    frzAtt = std::make_unique<ButtonAtt>(processor.apvts, AutoGainProcessor::paramFreeze, freezeButton);
    slAtt = std::make_unique<ButtonAtt>(processor.apvts, AutoGainProcessor::paramStereoLink, stereoLinkButton);
    psAtt = std::make_unique<ComboAtt>(processor.apvts, AutoGainProcessor::paramPreSource, preSourceCombo);

    startTimerHz(30);
    setSize(700, 450);
    setResizable(true, true);
}

// FIXED: Matches header declaration (no "= default")
AutoGainEditor::~AutoGainEditor() { stopTimer(); }
void AutoGainEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A1A));
}
void AutoGainEditor::DualMeter::setValues(float g, float r)
{
    if (gainVal != g || redVal != r) {
        gainVal = g;
        redVal = r;
        repaint();
    }
}

// FIXED: Only ONE paint() definition. Removed the duplicate from the original file.
void AutoGainEditor::DualMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour(findColour(backgroundColourId));
    g.fillRoundedRectangle(b, 4.0f);

    g.setColour(findColour(unityMarkerColourId));
    g.drawVerticalLine(juce::roundToInt(b.getX() + b.getWidth() * 0.5f), b.getY(), b.getBottom());

    if (redVal > 0.01f) {
        auto rb = b.withTrimmedTop(b.getHeight() * 0.5f);
        rb.setWidth(rb.getWidth() * redVal);
        g.setColour(findColour(reductionForegroundColourId));
        g.fillRoundedRectangle(rb, 2.0f);
    }

    if (gainVal < 0.99f) {
        auto gb = b.withTrimmedBottom(b.getHeight() * 0.5f);
        gb.setWidth(gb.getWidth() * gainVal);
        g.setColour(findColour(gainForegroundColourId));
        g.fillRoundedRectangle(gb, 2.0f);
    }
}

void AutoGainEditor::resized()
{
    auto b = getLocalBounds().reduced(12);
    auto top = b.removeFromTop(30);
    activeButton.setBounds(top.removeFromLeft(80));
    freezeButton.setBounds(top.removeFromLeft(80));
    preSourceCombo.setBounds(top.removeFromLeft(140));
    stereoLinkButton.setBounds(top);

    auto row = b.removeFromTop(110);
    int w = row.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto col = row.removeFromLeft(w);
        auto knobs = col.removeFromTop(90);
        auto labels = col.removeFromBottom(20);
        if (i == 0) { targetKnob.setBounds(knobs); targetLabel.setBounds(labels); }
        else if (i == 1) { attackKnob.setBounds(knobs); attackLabel.setBounds(labels); }
        else if (i == 2) { releaseKnob.setBounds(knobs); releaseLabel.setBounds(labels); }
        else { dryWetKnob.setBounds(knobs); dryWetLabel.setBounds(labels); }
    }

    auto mid = b.removeFromTop(100);
    w = mid.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto col = mid.removeFromLeft(w);
        auto knobs = col.removeFromTop(80);
        auto labels = col.removeFromBottom(20);
        if (i == 0) { thresholdKnob.setBounds(knobs); thresholdLabel.setBounds(labels); }
        else if (i == 1) { kneeKnob.setBounds(knobs); kneeLabel.setBounds(labels); }
        else if (i == 2) { maxBoostKnob.setBounds(knobs); maxBoostLabel.setBounds(labels); }
        else { maxCutKnob.setBounds(knobs); maxCutLabel.setBounds(labels); }
    }

    auto adv = b.removeFromTop(90);
    w = adv.getWidth() / 2;
    lookaheadKnob.setBounds(adv.removeFromLeft(w).removeFromTop(80));
    lookaheadLabel.setBounds(adv.removeFromLeft(w).removeFromBottom(20));
    sidechainHPFKnob.setBounds(adv.removeFromTop(80));
    hpfLabel.setBounds(adv.removeFromBottom(20));

    mainMeter.setBounds(b.reduced(0, 10));
}

void AutoGainEditor::timerCallback()
{
    float gainDB = processor.getCurrentGainDB();
    float redDB = processor.getCurrentGainReductionDB();

    float gMap = gainDB < 0 ? juce::jmap(gainDB, -24.0f, 0.0f, 0.0f, 0.5f)
        : juce::jmap(gainDB, 0.0f, 12.0f, 0.5f, 1.0f);
    float rMap = redDB < 0 ? juce::jmap(redDB, -24.0f, 0.0f, 0.0f, 0.5f)
        : juce::jmap(redDB, 0.0f, 12.0f, 0.5f, 1.0f);

    mainMeter.setValues(juce::jlimit(0.0f, 1.0f, gMap), juce::jlimit(0.0f, 1.0f, rMap));
}