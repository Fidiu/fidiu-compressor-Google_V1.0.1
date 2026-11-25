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
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
};

class FidiuCompressorEditor : public juce::AudioProcessorEditor, public juce::TooltipClient, private juce::Timer {
public:
    explicit FidiuCompressorEditor(FidiuCompressorProcessor&);
    ~FidiuCompressorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::String getTooltip() override;

private:
    void timerCallback() override;
    void setChoiceParam(const juce::String& paramId, int index);
    int getChoiceParam(const juce::String& paramId) const;
    void applyPreset(int index);
    void stepPreset(int dir);
    void startSignalAnalysis();
    void sampleAnalysisFrame();
    void finalizeSignalAnalysis();
    void paintWaveformDisplay(juce::Graphics&, juce::Rectangle<float> area);
    void paintSidechainDisplay(juce::Graphics&, juce::Rectangle<float> area);
    void paintKneeOverlay(juce::Graphics&, juce::Rectangle<float> area);
    void paintMeters(juce::Graphics&, juce::Rectangle<float> area);
    void updateButtonStates();
    void applyTooltipLanguage();

    FidiuCompressorProcessor& audioProcessor;
    FidiuLookAndFeel fidiuLook;

    enum class DisplayMode { Dynamics = 0, Sidechain = 1 };
    enum class DragTarget { None = 0, Threshold, Hpf, Lpf, Eq };
    DisplayMode displayMode = DisplayMode::Dynamics;
    DragTarget dragTarget = DragTarget::None;
    bool showKnee = true;
    int copyFlashFrames = 0;
    int lastAppliedLang = -1;

    bool isAnalyzing = false;
    bool hasSuggestion = false;
    int analyzeFramesLeft = 0;
    int analyzeActiveFrames = 0;
    std::array<double, 5> bandAccum {};
    double centroidNumer = 0.0, centroidDenom = 0.0;
    double crestAccum = 0.0, sideAccum = 0.0;
    juce::String analyzedTitle, analyzedSub;
    std::array<int, 3> sugPresetIndices { 0, 1, 3 };

    juce::Rectangle<int> displayBounds;
    juce::Rectangle<int> metersBounds;
    juce::Rectangle<int> consoleBounds;
    juce::Rectangle<int> scRailBounds;
    juce::Rectangle<int> suggestionBounds;
    std::array<int, 5> topDividers {};
    std::array<int, 3> conDividers {};
    std::array<int, 4> railDividers {};

    // Top bar controls
    juce::TextButton presetPrevBtn { "<" };
    juce::ComboBox presetBox;
    juce::TextButton presetNextBtn { ">" };
    juce::TextButton bankABtn { "A" };
    juce::TextButton bankBBtn { "B" };
    juce::TextButton copyAbBtn { "Copy" };
    juce::TextButton resetBtn { "Reset" };
    juce::TextButton analyzeBtn { "Analyze Signal" };
    juce::TextButton langBtn { "EN" };
    juce::TextButton levelMatchBtn { "Level Match" };
    juce::TextButton bypassBtn { "Bypass" };

    // Analyzer suggestion controls
    std::array<juce::TextButton, 3> sugPresetBtns;
    juce::TextButton sugCloseBtn { "x" };

    // Display mode buttons
    juce::TextButton dispModeBtn { "Display" };
    juce::TextButton scModeBtn { "Sidechain EQ" };
    juce::TextButton kneeToggleBtn { "Knee" };

    // Style & Coloriture buttons
    std::array<juce::TextButton, 8> styleBtns;
    std::array<juce::TextButton, 4> colorBtns;

    // Oversampling, Channel Mode & Slope & Sidechain buttons
    std::array<juce::TextButton, 3> osBtns;
    std::array<juce::TextButton, 3> chModeBtns;
    std::array<juce::TextButton, 4> hpfSlopeBtns;
    std::array<juce::TextButton, 4> lpfSlopeBtns;

    juce::TextButton autoRelBtn { "Auto" };
    juce::TextButton parallelBtn { "Parallel" };
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
    juce::Slider charSlider, mixSlider, gainSlider, dryGainSlider;
    juce::Slider lookaheadSlider, linkSlider, msSlider;
    juce::Slider scHpfSlider, scLpfSlider, scEqFreqSlider, scEqGainSlider, scEqQSlider;

    std::vector<std::pair<juce::Slider*, juce::String>> knobLabels;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        threshAtt, ratioAtt, kneeAtt, rangeAtt, attackAtt, holdAtt, releaseAtt,
        charAtt, mixAtt, gainAtt, dryGainAtt, laAtt, linkAtt, msAtt,
        scHpfAtt, scLpfAtt, scEqFreqAtt, scEqGainAtt, scEqQAtt;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        levelMatchAtt, bypassAtt, autoRelAtt, parallelAtt, autoGainAtt, lookaheadAtt, scAudAtt, scHpfAttBtn, scLpfAttBtn, scEqAttBtn;

    juce::dsp::FFT scFft { 12 };
    juce::dsp::WindowingFunction<float> scWindow { 4096, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 8192> fftWork {};
    std::array<float, 8192> inFftWork {};
    std::array<float, 2048> smoothedBins {};

    juce::TooltipWindow tooltipWindow { this, 3000 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FidiuCompressorEditor)
};
