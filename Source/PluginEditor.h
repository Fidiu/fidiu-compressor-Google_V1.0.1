#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
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
    void applyPreset(int index, bool calibrateThreshold = false);
    void applySidechainPreset(int scIdx);
    void rebuildScPresetMenu();
    void syncSidechainPresetBox();
    float computeCalibratedThreshold(int presetIdx, double avgRmsDb, double avgCrestDb, double targetGrDb) const;
    void rebuildPresetMenu(int selectItemId = -1);
    void openSavePresetDialog();
    void closeSavePresetDialog();
    void commitSavePreset();
    juce::File getUserPresetsDirectory() const;
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
    bool hasValidAnalysis = false;
    int analyzeFramesLeft = 0;
    int analyzeActiveFrames = 0;
    double ultraSubAccum = 0.0, presenceAccum = 0.0, airAccum = 0.0;
    std::array<double, 5> bandAccum {};
    double centroidNumer = 0.0, centroidDenom = 0.0;
    double rmsAccum = 0.0, crestAccum = 0.0, sideAccum = 0.0;
    double rmsMin = 0.0, rmsMax = -96.0;
    std::array<double, 5> prevFrameBands {};
    bool hasPrevFrameBands = false;
    double fluxAccum = 0.0;
    int fluxCount = 0;
    std::vector<double> frameSubRatios;
    double lastAvgRmsDb = -18.0, lastAvgCrestDb = 10.0, lastTargetGrDb = 3.5;
    juce::String analyzedTitle, analyzedSub;
    std::array<int, 3> sugPresetIndices { 1, 2, 4 };
    static constexpr int kSavePresetMenuId = 9001;
    static constexpr int kUserPresetIdBase = 1000;
    int lastValidPresetId = 1;
    std::vector<int> selectablePresetIds;
    std::vector<juce::File> userPresetFiles;

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

    // Display mode buttons & Sidechain preset selector
    juce::TextButton dispModeBtn { "Display" };
    juce::TextButton scModeBtn { "Sidechain EQ" };
    juce::TextButton kneeToggleBtn { "Knee" };
    juce::ComboBox scPresetBox;

    // Style & Coloriture buttons
    std::array<juce::TextButton, 8> styleBtns;
    std::array<juce::TextButton, 4> colorBtns;

    // Oversampling, Channel Mode & Slope & Sidechain buttons
    std::array<juce::TextButton, 3> osBtns;
    std::array<juce::TextButton, 3> chModeBtns;
    std::array<juce::TextButton, 3> scMsBtns;
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

    struct SavePresetOverlay : public juce::Component {
        std::function<void()> onConfirm;
        std::function<void()> onCancel;
        bool isRomanian = false;
        juce::TextEditor nameEditor;
        juce::TextButton saveBtn { "Save" };
        juce::TextButton cancelBtn { "Cancel" };

        SavePresetOverlay() {
            nameEditor.setMultiLine(false);
            nameEditor.setReturnKeyStartsNewLine(false);
            nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0e1117));
            nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2d3340));
            nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff38d4ff));
            nameEditor.setFont(juce::FontOptions(13.5f));
            nameEditor.onReturnKey = [this] { if (onConfirm) onConfirm(); };
            nameEditor.onEscapeKey = [this] { if (onCancel) onCancel(); };
            addAndMakeVisible(nameEditor);

            saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff5430));
            saveBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            saveBtn.onClick = [this] { if (onConfirm) onConfirm(); };
            addAndMakeVisible(saveBtn);

            cancelBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252a34));
            cancelBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
            cancelBtn.onClick = [this] { if (onCancel) onCancel(); };
            addAndMakeVisible(cancelBtn);
        }

        juce::Rectangle<int> getCardBounds() const {
            return juce::Rectangle<int>(0, 0, 390, 152).withCentre(getLocalBounds().getCentre());
        }

        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colour(0xb8080a0f));
            const auto card = getCardBounds().toFloat();
            g.setColour(juce::Colour(0xff161a23));
            g.fillRoundedRectangle(card, 7.0f);
            g.setColour(juce::Colour(0xff38d4ff).withAlpha(0.55f));
            g.drawRoundedRectangle(card, 7.0f, 1.2f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            g.drawText(isRomanian ? juce::String::fromUTF8("Salvează Preset Ca...") : juce::String("Save Preset As..."),
                       static_cast<int>(card.getX() + 16), static_cast<int>(card.getY() + 12), 260, 20, juce::Justification::left, false);

            g.setColour(juce::Colour(0xff9aa4b6));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(isRomanian ? juce::String::fromUTF8("Nume preset (salvat în Preseturi Utilizator):")
                                  : juce::String("Preset name (saved to User Presets):"),
                       static_cast<int>(card.getX() + 16), static_cast<int>(card.getY() + 36), 358, 16, juce::Justification::left, false);
        }

        void resized() override {
            const auto card = getCardBounds();
            nameEditor.setBounds(card.getX() + 16, card.getY() + 58, card.getWidth() - 32, 30);
            cancelBtn.setBounds(card.getRight() - 188, card.getBottom() - 42, 82, 26);
            saveBtn.setBounds(card.getRight() - 98, card.getBottom() - 42, 82, 26);
        }

        void mouseDown(const juce::MouseEvent& e) override {
            if (!getCardBounds().contains(e.getPosition())) {
                if (onCancel) onCancel();
            }
        }
    };
    SavePresetOverlay savePresetOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FidiuCompressorEditor)
};
