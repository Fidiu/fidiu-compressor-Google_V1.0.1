#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class FidiuLookAndFeel : public juce::LookAndFeel_V4 {
public:
    FidiuLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;
};

class FidiuCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit FidiuCompressorEditor(FidiuCompressorProcessor&);
    ~FidiuCompressorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setChoiceParam(const juce::String& paramId, int index);
    int getChoiceParam(const juce::String& paramId) const;
    void applyPreset(int index);
    void paintWaveformDisplay(juce::Graphics&, juce::Rectangle<float> area);
    void paintSidechainDisplay(juce::Graphics&, juce::Rectangle<float> area);
    void paintKneeOverlay(juce::Graphics&, juce::Rectangle<float> area);
    void paintMeters(juce::Graphics&, juce::Rectangle<float> area);
    void updateButtonStates();

    FidiuCompressorProcessor& audioProcessor;
    FidiuLookAndFeel fidiuLook;

    enum class DisplayMode { Dynamics = 0, Sidechain = 1 };
    enum class DragTarget { None = 0, Threshold, Hpf, Lpf, Eq };
    DisplayMode displayMode = DisplayMode::Dynamics;
    DragTarget dragTarget = DragTarget::None;
    bool showKnee = true;

    juce::Rectangle<int> displayBounds;
    juce::Rectangle<int> metersBounds;
    juce::Rectangle<int> consoleBounds;
    juce::Rectangle<int> scRailBounds;

    // Top bar controls
    juce::ComboBox presetBox;
    juce::TextButton bypassBtn { "Bypass" };

    // Display mode buttons
    juce::TextButton dispModeBtn { "Display" };
    juce::TextButton scModeBtn { "Sidechain EQ" };
    juce::TextButton kneeToggleBtn { "Knee" };

    // Style & Coloriture buttons
    std::array<juce::TextButton, 8> styleBtns;
    std::array<juce::TextButton, 4> colorBtns;

    // Oversampling & Slope & Sidechain buttons
    std::array<juce::TextButton, 3> osBtns;
    std::array<juce::TextButton, 4> hpfSlopeBtns;
    std::array<juce::TextButton, 4> lpfSlopeBtns;

    juce::TextButton autoRelBtn { "Auto" };
    juce::TextButton autoGainBtn { "Auto Gain" };
    juce::TextButton lookaheadBtn { "Lookahead" };
    juce::TextButton scIntBtn { "Int SC" };
    juce::TextButton scExtBtn { "Ext SC" };
    juce::TextButton scAudBtn { "Audition" };
    juce::TextButton scHpfBtn { "HPF" };
    juce::TextButton scLpfBtn { "LPF" };
    juce::TextButton scEqBtn { "SC Bell" };

    // Rotary knobs
    juce::Slider threshSlider, ratioSlider, kneeSlider, rangeSlider;
    juce::Slider attackSlider, holdSlider, releaseSlider;
    juce::Slider charSlider, mixSlider, gainSlider;
    juce::Slider lookaheadSlider, linkSlider, msSlider;
    juce::Slider scHpfSlider, scLpfSlider, scEqFreqSlider, scEqGainSlider, scEqQSlider;

    std::vector<std::pair<juce::Slider*, juce::String>> knobLabels;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        threshAtt, ratioAtt, kneeAtt, rangeAtt, attackAtt, holdAtt, releaseAtt,
        charAtt, mixAtt, gainAtt, laAtt, linkAtt, msAtt,
        scHpfAtt, scLpfAtt, scEqFreqAtt, scEqGainAtt, scEqQAtt;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        bypassAtt, autoRelAtt, autoGainAtt, lookaheadAtt, scAudAtt, scHpfAttBtn, scLpfAttBtn, scEqAttBtn;

    juce::dsp::FFT scFft { 10 };
    juce::dsp::WindowingFunction<float> scWindow { 1024, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2048> fftWork {};
    std::array<float, 512> smoothedBins {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FidiuCompressorEditor)
};
