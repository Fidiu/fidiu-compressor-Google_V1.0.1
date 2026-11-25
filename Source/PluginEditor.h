#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class FidiuCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit FidiuCompressorEditor(FidiuCompressorProcessor&);
    ~FidiuCompressorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    FidiuCompressorProcessor& audioProcessor;

    juce::Slider threshSlider, ratioSlider, kneeSlider, rangeSlider;
    juce::Slider attackSlider, holdSlider, releaseSlider;
    juce::Slider charSlider, mixSlider, gainSlider, lookaheadSlider, scHpfSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        threshAtt, ratioAtt, kneeAtt, rangeAtt, attackAtt, holdAtt, releaseAtt,
        charAtt, mixAtt, gainAtt, laAtt, scHpfAtt;

    float grDisplay = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FidiuCompressorEditor)
};
