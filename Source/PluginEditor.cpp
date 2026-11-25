#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour kBgOuter     (0xff1c2029);
const juce::Colour kBgTopBar    (0xff232732);
const juce::Colour kBgConsole   (0xff1d212a);
const juce::Colour kBgRail      (0xff181b22);
const juce::Colour kBorder      (0xff303644);
const juce::Colour kCyan        (0xff38d4ff);
const juce::Colour kOrange      (0xffff5430);
const juce::Colour kAmber       (0xfff59e0b);
const juce::Colour kSilver      (0xffd5dce6);
const juce::Colour kMutedText   (0xff9aa4b6);

float computeCurveOutDb(float inDb, float thresh, float ratio, float knee, float range) {
    if (ratio <= 1.001f) return inDb;
    const float slope = 1.0f - 1.0f / (ratio >= 99.0f ? 1000.0f : ratio);
    const float halfKnee = knee * 0.5f;
    float gr = 0.0f;
    if (knee > 0.05f && inDb > thresh - halfKnee && inDb < thresh + halfKnee) {
        const float d = inDb - (thresh - halfKnee);
        gr = slope * (d * d) / (2.0f * knee);
    } else if (inDb >= thresh + halfKnee) {
        gr = slope * (inDb - thresh);
    }
    gr = std::clamp(gr, 0.0f, range);
    return inDb - gr;
}

float calcFilterDb(float freq, bool hpfOn, float hpfFreq, int hpfSlope,
                   bool lpfOn, float lpfFreq, int lpfSlope,
                   bool eqOn, float eqFreq, float eqGain, float eqQ) {
    float db = 0.0f;
    if (hpfOn) {
        const float order = static_cast<float>(hpfSlope) / 6.0f;
        const float ratio = hpfFreq / std::max(1.0f, freq);
        db -= 10.0f * std::log10(1.0f + std::pow(ratio, 2.0f * order));
    }
    if (lpfOn) {
        const float order = static_cast<float>(lpfSlope) / 6.0f;
        const float ratio = freq / std::max(1.0f, lpfFreq);
        db -= 10.0f * std::log10(1.0f + std::pow(ratio, 2.0f * order));
    }
    if (eqOn && std::abs(eqGain) > 0.05f) {
        const float oct = std::log2(freq / std::max(1.0f, eqFreq));
        const float bw = 1.0f / std::max(0.25f, eqQ);
        db += eqGain * std::exp(-0.5f * std::pow(oct / (bw * 0.55f), 2.0f));
    }
    return std::clamp(db, -36.0f, 24.0f);
}

float freqToX(float freq, juce::Rectangle<float> r) {
    const float minF = std::log10(20.0f);
    const float maxF = std::log10(20000.0f);
    const float n = (std::log10(std::clamp(freq, 20.0f, 20000.0f)) - minF) / (maxF - minF);
    return r.getX() + n * r.getWidth();
}

float xToFreq(float x, juce::Rectangle<float> r) {
    const float minF = std::log10(20.0f);
    const float maxF = std::log10(20000.0f);
    const float n = std::clamp((x - r.getX()) / std::max(1.0f, r.getWidth()), 0.0f, 1.0f);
    return std::pow(10.0f, minF + n * (maxF - minF));
}
} // namespace

FidiuLookAndFeel::FidiuLookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff14171f));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2d3340));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff181c24));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe2e8f0));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2c3342));
}

void FidiuLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label) {
    g.setColour(juce::Colour(0xffe2e8f0));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(label.getText(), label.getLocalBounds(), juce::Justification::centred, false);
}

void FidiuLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(button.findColour(button.getToggleState()
                                      ? juce::TextButton::textColourOnId
                                      : juce::TextButton::textColourOffId)
                      .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    auto area = button.getLocalBounds().reduced(2, 1);
    g.drawFittedText(button.getButtonText(), area, juce::Justification::centred, 1, 0.85f);
}

void FidiuLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider) {
    const float size = static_cast<float>(std::min(width, height));
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float arcR = size * 0.5f - 3.5f;
    const float wellR = arcR - 3.2f;
    const float dialR = wellR - 2.2f;

    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const bool enabled = slider.isEnabled();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Outer groove track
    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff12151b));
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar or unipolar active arc
    const bool bipolar = static_cast<bool>(slider.getProperties().getWithDefault("bipolar", false));
    float startA = rotaryStartAngle;
    float endA = angle;
    if (bipolar) {
        const float midA = 0.5f * (rotaryStartAngle + rotaryEndAngle);
        startA = std::min(midA, angle);
        endA = std::max(midA, angle);
    }

    if (std::abs(endA - startA) > 0.01f) {
        juce::Path valArc;
        valArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startA, endA, true);
        g.setColour((enabled ? accent : juce::Colour(0xff475569)).withAlpha(0.28f));
        g.strokePath(valArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(enabled ? accent : juce::Colour(0xff475569));
        g.strokePath(valArc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Recessed dial well
    g.setColour(juce::Colour(0xff12141a));
    g.fillEllipse(cx - wellR, cy - wellR, wellR * 2.0f, wellR * 2.0f);

    // 3D machined gunmetal cap
    juce::ColourGradient capGrad(juce::Colour(0xff434a59), cx - dialR * 0.6f, cy - dialR * 0.6f,
                                 juce::Colour(0xff1b1e26), cx + dialR * 0.7f, cy + dialR * 0.7f, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(cx - dialR, cy - dialR, dialR * 2.0f, dialR * 2.0f);

    g.setColour(slider.isMouseOverOrDragging() ? accent.withAlpha(0.75f) : juce::Colour(0xff383f4d));
    g.drawEllipse(cx - dialR, cy - dialR, dialR * 2.0f, dialR * 2.0f, 1.0f);

    const float innerR = std::max(3.0f, dialR - 3.0f);
    juce::ColourGradient innerGrad(juce::Colour(0xff3b414f), cx - innerR * 0.4f, cy - innerR * 0.4f,
                                   juce::Colour(0xff1a1d24), cx + innerR * 0.6f, cy + innerR * 0.6f, true);
    g.setGradientFill(innerGrad);
    g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    // Crisp radial pointer
    const float pStart = dialR * 0.30f;
    const float pEnd = dialR - 1.5f;
    const float sinA = std::sin(angle);
    const float cosA = -std::cos(angle);
    g.setColour(enabled ? juce::Colour(0xfff1f5f9) : juce::Colour(0xff64748b));
    g.drawLine(cx + sinA * pStart, cy + cosA * pStart, cx + sinA * pEnd, cy + cosA * pEnd, 2.0f);
}

FidiuCompressorEditor::FidiuCompressorEditor(FidiuCompressorProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&fidiuLook);
    setSize(1220, 700);
    setResizable(true, true);
    setResizeLimits(1000, 600, 1680, 980);

    smoothedBins.fill(-90.0f);

    presetBox.addItem("Studio Bus Glue", 1);
    presetBox.addItem("Mastering Transparent", 2);
    presetBox.addItem("Vocal Forward Opto", 3);
    presetBox.addItem("Parallel Drum Punch", 4);
    presetBox.addItem("Zero-Overshoot Lookahead", 5);
    presetBox.addItem("External Kick Ducking", 6);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPreset(presetBox.getSelectedId() - 1); };
    addAndMakeVisible(presetBox);

    auto styleBtn = [this](juce::TextButton& b, const juce::Colour& onCol) {
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14171f));
        b.setColour(juce::TextButton::buttonOnColourId, onCol);
        b.setColour(juce::TextButton::textColourOffId, kMutedText);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(b);
    };

    styleBtn(bankABtn, juce::Colour(0xff164e63));
    styleBtn(bankBBtn, kOrange.darker(0.2f));
    styleBtn(copyAbBtn, juce::Colour(0xff2c3342));
    styleBtn(resetBtn, juce::Colour(0xff2c3342));
    styleBtn(levelMatchBtn, juce::Colour(0xff164e63));
    styleBtn(bypassBtn, kAmber.darker(0.35f));
    styleBtn(dispModeBtn, juce::Colour(0xff2c3342));
    styleBtn(scModeBtn, juce::Colour(0xff164e63));
    styleBtn(kneeToggleBtn, juce::Colour(0xff2c3342));

    bankABtn.setClickingTogglesState(false);
    bankBBtn.setClickingTogglesState(false);
    copyAbBtn.setClickingTogglesState(false);
    resetBtn.setClickingTogglesState(false);

    bankABtn.onClick = [this] { audioProcessor.selectBank(0); updateButtonStates(); repaint(); };
    bankBBtn.onClick = [this] { audioProcessor.selectBank(1); updateButtonStates(); repaint(); };
    copyAbBtn.onClick = [this] {
        audioProcessor.copyActiveBankToOther();
        copyAbBtn.setButtonText(audioProcessor.getActiveBank() == 0 ? "A -> B" : "B -> A");
        copyFlashFrames = 55;
    };
    resetBtn.onClick = [this] { audioProcessor.resetToDefault(); updateButtonStates(); repaint(); };

    dispModeBtn.setClickingTogglesState(false);
    scModeBtn.setClickingTogglesState(false);
    dispModeBtn.onClick = [this] { displayMode = DisplayMode::Dynamics; updateButtonStates(); repaint(); };
    scModeBtn.onClick = [this] { displayMode = DisplayMode::Sidechain; updateButtonStates(); repaint(); };
    kneeToggleBtn.setToggleState(true, juce::dontSendNotification);
    kneeToggleBtn.onClick = [this] { showKnee = kneeToggleBtn.getToggleState(); repaint(); };

    static const char* styleNames[8] = { "Clean", "Classic", "Opto", "Vocal", "Mastering", "Bus", "Punch", "Pumping" };
    for (int i = 0; i < 8; ++i) {
        styleBtn(styleBtns[static_cast<size_t>(i)], kOrange);
        styleBtns[static_cast<size_t>(i)].setButtonText(styleNames[i]);
        styleBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        styleBtns[static_cast<size_t>(i)].onClick = [this, i] { setChoiceParam("style", i); };
    }

    static const char* colorNames[4] = { "Pure", "Tube", "Tape", "Xfmr" };
    for (int i = 0; i < 4; ++i) {
        styleBtn(colorBtns[static_cast<size_t>(i)], kAmber.darker(0.4f));
        colorBtns[static_cast<size_t>(i)].setButtonText(colorNames[i]);
        colorBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        colorBtns[static_cast<size_t>(i)].onClick = [this, i] { setChoiceParam("colorMode", i); };
    }

    static const char* osNames[3] = { "1x", "2x", "4x" };
    for (int i = 0; i < 3; ++i) {
        styleBtn(osBtns[static_cast<size_t>(i)], juce::Colour(0xff164e63));
        osBtns[static_cast<size_t>(i)].setButtonText(osNames[i]);
        osBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        osBtns[static_cast<size_t>(i)].onClick = [this, i] { setChoiceParam("oversampling", i); };
    }

    static const char* chModeNames[3] = { "Stereo", "Mid", "Side" };
    const std::array<juce::Colour, 3> chModeCols = { juce::Colour(0xff2c3342), juce::Colour(0xff164e63), kOrange.darker(0.2f) };
    for (int i = 0; i < 3; ++i) {
        styleBtn(chModeBtns[static_cast<size_t>(i)], chModeCols[static_cast<size_t>(i)]);
        chModeBtns[static_cast<size_t>(i)].setButtonText(chModeNames[i]);
        chModeBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        chModeBtns[static_cast<size_t>(i)].onClick = [this, i] { setChoiceParam("channelMode", i); };
    }

    static const char* slopeNames[4] = { "6", "12", "24", "48" };
    for (int i = 0; i < 4; ++i) {
        styleBtn(hpfSlopeBtns[static_cast<size_t>(i)], juce::Colour(0xff164e63));
        hpfSlopeBtns[static_cast<size_t>(i)].setButtonText(slopeNames[i]);
        hpfSlopeBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        hpfSlopeBtns[static_cast<size_t>(i)].onClick = [this, i] { setChoiceParam("scHpfSlope", i); };

        styleBtn(lpfSlopeBtns[static_cast<size_t>(i)], juce::Colour(0xff164e63));
        lpfSlopeBtns[static_cast<size_t>(i)].setButtonText(slopeNames[i]);
        lpfSlopeBtns[static_cast<size_t>(i)].setClickingTogglesState(false);
        lpfSlopeBtns[static_cast<size_t>(i)].onClick = [this, i] { setChoiceParam("scLpfSlope", i); };
    }

    styleBtn(autoRelBtn, juce::Colour(0xff164e63));
    styleBtn(parallelBtn, kOrange.darker(0.2f));
    styleBtn(autoGainBtn, juce::Colour(0xff164e63));
    styleBtn(lookaheadBtn, juce::Colour(0xff164e63));
    styleBtn(scIntBtn, juce::Colour(0xff2c3342));
    styleBtn(scExtBtn, juce::Colour(0xff164e63));
    styleBtn(scAudBtn, kAmber.darker(0.35f));
    styleBtn(scHpfBtn, juce::Colour(0xff164e63));
    styleBtn(scLpfBtn, juce::Colour(0xff164e63));
    styleBtn(scEqBtn, kAmber.darker(0.45f));

    scIntBtn.setClickingTogglesState(false);
    scExtBtn.setClickingTogglesState(false);
    scIntBtn.onClick = [this] {
        if (auto* param = audioProcessor.apvts.getParameter("scExt"))
            param->setValueNotifyingHost(0.0f);
    };
    scExtBtn.onClick = [this] {
        if (auto* param = audioProcessor.apvts.getParameter("scExt"))
            param->setValueNotifyingHost(1.0f);
    };

    auto initKnob = [this](juce::Slider& s, const juce::String& label, const juce::Colour& col, const juce::String& suffix, bool bipolar = false) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 15);
        s.setColour(juce::Slider::rotarySliderFillColourId, col);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffdce3ee));
        s.setTextValueSuffix(suffix);
        s.getProperties().set("bipolar", bipolar);
        addAndMakeVisible(s);
        knobLabels.push_back({ &s, label });
    };

    initKnob(threshSlider, "THRESHOLD", kCyan, " dB");
    initKnob(ratioSlider, "RATIO", kOrange, ":1");
    initKnob(kneeSlider, "KNEE", kOrange, " dB");
    initKnob(rangeSlider, "RANGE", kOrange, " dB");
    initKnob(attackSlider, "ATTACK", kCyan, " ms");
    initKnob(holdSlider, "HOLD", kCyan, " ms");
    initKnob(releaseSlider, "RELEASE", kCyan, " ms");
    initKnob(charSlider, "DRIVE", kAmber, "%");
    initKnob(mixSlider, "MIX", kSilver, "%");
    initKnob(gainSlider, "WET GAIN", kCyan, " dB", true);
    initKnob(dryGainSlider, "DRY GAIN", kSilver, " dB", true);
    dryGainSlider.textFromValueFunction = [](double v) {
        if (v <= -35.8) return juce::String("-inf dB");
        return (v >= 0.0 ? juce::String("+") : juce::String("")) + juce::String(v, 1) + " dB";
    };

    initKnob(lookaheadSlider, "LOOKAHEAD", kCyan, " ms");
    initKnob(linkSlider, "LINK", kSilver, "%");
    initKnob(msSlider, "M/S BAL", kCyan, "%", true);
    initKnob(scHpfSlider, "HPF", kCyan, " Hz");
    initKnob(scLpfSlider, "LPF", kCyan, " Hz");
    initKnob(scEqFreqSlider, "FREQ", kAmber, " Hz");
    initKnob(scEqGainSlider, "GAIN", kAmber, " dB", true);
    initKnob(scEqQSlider, "Q", kSilver, "");

    threshAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "threshold", threshSlider);
    ratioAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "ratio", ratioSlider);
    kneeAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "knee", kneeSlider);
    rangeAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "range", rangeSlider);
    attackAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "attack", attackSlider);
    holdAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hold", holdSlider);
    releaseAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "release", releaseSlider);
    charAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "character", charSlider);
    mixAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "mix", mixSlider);
    gainAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "wetGain", gainSlider);
    dryGainAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "dryGain", dryGainSlider);
    laAtt        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "lookaheadMs", lookaheadSlider);
    linkAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "stereoLink", linkSlider);
    msAtt        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "msBalance", msSlider);
    scHpfAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "scHpfFreq", scHpfSlider);
    scLpfAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "scLpfFreq", scLpfSlider);
    scEqFreqAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "scEqFreq", scEqFreqSlider);
    scEqGainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "scEqGain", scEqGainSlider);
    scEqQAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "scEqQ", scEqQSlider);

    levelMatchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "levelMatch", levelMatchBtn);
    bypassAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "bypass", bypassBtn);
    autoRelAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "autoRelease", autoRelBtn);
    parallelAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "parallelMode", parallelBtn);
    autoGainAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "autoGain", autoGainBtn);
    lookaheadAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "lookaheadOn", lookaheadBtn);
    scAudAtt      = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "scAudition", scAudBtn);
    scHpfAttBtn   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "scHpfOn", scHpfBtn);
    scLpfAttBtn   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "scLpfOn", scLpfBtn);
    scEqAttBtn    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "scEqOn", scEqBtn);

    updateButtonStates();
    startTimerHz(60);
}

FidiuCompressorEditor::~FidiuCompressorEditor() {
    setLookAndFeel(nullptr);
}

void FidiuCompressorEditor::setChoiceParam(const juce::String& paramId, int index) {
    if (auto* param = audioProcessor.apvts.getParameter(paramId)) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
        param->endChangeGesture();
    }
}

int FidiuCompressorEditor::getChoiceParam(const juce::String& paramId) const {
    if (auto* raw = audioProcessor.apvts.getRawParameterValue(paramId))
        return static_cast<int>(std::round(raw->load()));
    return 0;
}

void FidiuCompressorEditor::applyPreset(int idx) {
    auto setVal = [this](const juce::String& id, float v) {
        if (auto* param = audioProcessor.apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(v));
    };

    setChoiceParam("channelMode", 0);
    setVal("dryGain", 0.0f);
    setVal("parallelMode", idx == 3 ? 1.0f : 0.0f);

    if (idx == 0) { // Studio Bus Glue
        setChoiceParam("style", 5); setVal("threshold", -18.0f); setVal("ratio", 4.0f); setVal("knee", 12.0f);
        setVal("range", 36.0f); setVal("attack", 12.0f); setVal("hold", 8.0f); setVal("release", 140.0f);
        setVal("autoRelease", 1.0f); setVal("character", 24.0f); setChoiceParam("colorMode", 1);
        setVal("mix", 100.0f); setVal("wetGain", 2.4f); setVal("lookaheadOn", 1.0f); setVal("lookaheadMs", 4.0f);
        setChoiceParam("oversampling", 1); setVal("scHpfOn", 1.0f); setVal("scHpfFreq", 95.0f); setVal("scLpfOn", 0.0f); setVal("scEqOn", 0.0f);
    } else if (idx == 1) { // Mastering Transparent
        setChoiceParam("style", 4); setVal("threshold", -14.5f); setVal("ratio", 2.2f); setVal("knee", 24.0f);
        setVal("range", 12.0f); setVal("attack", 28.0f); setVal("hold", 0.0f); setVal("release", 220.0f);
        setVal("autoRelease", 1.0f); setVal("character", 14.0f); setChoiceParam("colorMode", 3);
        setVal("mix", 100.0f); setVal("wetGain", 1.5f); setVal("lookaheadOn", 1.0f); setVal("lookaheadMs", 2.5f);
        setChoiceParam("oversampling", 2); setVal("scHpfOn", 1.0f); setVal("scHpfFreq", 65.0f); setVal("scLpfOn", 0.0f); setVal("scEqOn", 0.0f);
    } else if (idx == 2) { // Vocal Forward Opto
        setChoiceParam("style", 3); setVal("threshold", -21.0f); setVal("ratio", 3.5f); setVal("knee", 16.0f);
        setVal("range", 24.0f); setVal("attack", 6.5f); setVal("hold", 12.0f); setVal("release", 160.0f);
        setVal("autoRelease", 1.0f); setVal("character", 32.0f); setChoiceParam("colorMode", 2);
        setVal("mix", 100.0f); setVal("wetGain", 4.0f); setVal("lookaheadOn", 1.0f); setVal("lookaheadMs", 3.0f);
        setChoiceParam("oversampling", 1); setVal("scHpfOn", 1.0f); setVal("scHpfFreq", 130.0f);
        setVal("scEqOn", 1.0f); setVal("scEqFreq", 3200.0f); setVal("scEqGain", 3.5f); setVal("scEqQ", 1.4f);
    } else if (idx == 3) { // Parallel Drum Punch
        setChoiceParam("style", 6); setVal("threshold", -26.0f); setVal("ratio", 10.0f); setVal("knee", 6.0f);
        setVal("range", 42.0f); setVal("attack", 18.0f); setVal("hold", 15.0f); setVal("release", 85.0f);
        setVal("autoRelease", 0.0f); setVal("character", 58.0f); setChoiceParam("colorMode", 1);
        setVal("mix", 52.0f); setVal("wetGain", 6.5f); setVal("lookaheadOn", 0.0f);
        setChoiceParam("oversampling", 2); setVal("scHpfOn", 1.0f); setVal("scHpfFreq", 110.0f);
        setVal("scLpfOn", 1.0f); setVal("scLpfFreq", 8500.0f);
    } else if (idx == 4) { // Zero-Overshoot Lookahead
        setChoiceParam("style", 0); setVal("threshold", -11.0f); setVal("ratio", 100.0f); setVal("knee", 1.5f);
        setVal("range", 24.0f); setVal("attack", 0.1f); setVal("hold", 10.0f); setVal("release", 75.0f);
        setVal("autoRelease", 0.0f); setVal("character", 8.0f); setChoiceParam("colorMode", 0);
        setVal("mix", 100.0f); setVal("wetGain", 3.0f); setVal("lookaheadOn", 1.0f); setVal("lookaheadMs", 10.0f);
        setChoiceParam("oversampling", 2); setVal("scHpfOn", 0.0f); setVal("scLpfOn", 0.0f); setVal("scEqOn", 0.0f);
    } else if (idx == 5) { // External Kick Ducking
        setChoiceParam("style", 7); setVal("threshold", -24.0f); setVal("ratio", 8.0f); setVal("knee", 8.0f);
        setVal("range", 30.0f); setVal("attack", 1.5f); setVal("hold", 25.0f); setVal("release", 175.0f);
        setVal("autoRelease", 0.0f); setVal("character", 28.0f); setChoiceParam("colorMode", 2);
        setVal("mix", 100.0f); setVal("wetGain", 2.0f); setVal("lookaheadOn", 1.0f); setVal("lookaheadMs", 2.0f);
        setVal("scExt", 1.0f); setVal("scHpfOn", 1.0f); setVal("scHpfFreq", 45.0f); setVal("scLpfOn", 1.0f); setVal("scLpfFreq", 450.0f);
    }
}

void FidiuCompressorEditor::updateButtonStates() {
    const int bank = audioProcessor.getActiveBank();
    bankABtn.setToggleState(bank == 0, juce::dontSendNotification);
    bankBBtn.setToggleState(bank == 1, juce::dontSendNotification);

    dispModeBtn.setToggleState(displayMode == DisplayMode::Dynamics, juce::dontSendNotification);
    scModeBtn.setToggleState(displayMode == DisplayMode::Sidechain, juce::dontSendNotification);
    kneeToggleBtn.setVisible(displayMode == DisplayMode::Dynamics);

    const int activeStyle = getChoiceParam("style");
    for (int i = 0; i < 8; ++i)
        styleBtns[static_cast<size_t>(i)].setToggleState(i == activeStyle, juce::dontSendNotification);

    const int activeColor = getChoiceParam("colorMode");
    for (int i = 0; i < 4; ++i)
        colorBtns[static_cast<size_t>(i)].setToggleState(i == activeColor, juce::dontSendNotification);

    const int activeOs = getChoiceParam("oversampling");
    for (int i = 0; i < 3; ++i)
        osBtns[static_cast<size_t>(i)].setToggleState(i == activeOs, juce::dontSendNotification);

    const int activeCh = getChoiceParam("channelMode");
    for (int i = 0; i < 3; ++i)
        chModeBtns[static_cast<size_t>(i)].setToggleState(i == activeCh, juce::dontSendNotification);
    msSlider.setEnabled(activeCh == 0);

    const int activeHpfSlope = getChoiceParam("scHpfSlope");
    const int activeLpfSlope = getChoiceParam("scLpfSlope");
    for (int i = 0; i < 4; ++i) {
        hpfSlopeBtns[static_cast<size_t>(i)].setToggleState(i == activeHpfSlope, juce::dontSendNotification);
        lpfSlopeBtns[static_cast<size_t>(i)].setToggleState(i == activeLpfSlope, juce::dontSendNotification);
    }

    const bool scExt = audioProcessor.apvts.getRawParameterValue("scExt")->load() > 0.5f;
    scIntBtn.setToggleState(!scExt, juce::dontSendNotification);
    scExtBtn.setToggleState(scExt, juce::dontSendNotification);

    lookaheadSlider.setEnabled(audioProcessor.apvts.getRawParameterValue("lookaheadOn")->load() > 0.5f);
    scHpfSlider.setEnabled(audioProcessor.apvts.getRawParameterValue("scHpfOn")->load() > 0.5f);
    scLpfSlider.setEnabled(audioProcessor.apvts.getRawParameterValue("scLpfOn")->load() > 0.5f);
    const bool eqOn = audioProcessor.apvts.getRawParameterValue("scEqOn")->load() > 0.5f;
    scEqFreqSlider.setEnabled(eqOn);
    scEqGainSlider.setEnabled(eqOn);
    scEqQSlider.setEnabled(eqOn);
}

void FidiuCompressorEditor::timerCallback() {
    updateButtonStates();
    if (copyFlashFrames > 0) {
        --copyFlashFrames;
        if (copyFlashFrames == 0)
            copyAbBtn.setButtonText("Copy");
    }
    if (displayMode == DisplayMode::Sidechain) {
        const int wIdx = audioProcessor.scRingIdx.load(std::memory_order_relaxed);
        for (int i = 0; i < 4096; ++i) {
            const int rIdx = (wIdx + i) % 4096;
            fftWork[static_cast<size_t>(i)] = audioProcessor.scFftRing[static_cast<size_t>(rIdx)].load(std::memory_order_relaxed);
        }
        std::fill(fftWork.begin() + 4096, fftWork.end(), 0.0f);
        scWindow.multiplyWithWindowingTable(fftWork.data(), 4096);
        scFft.performFrequencyOnlyForwardTransform(fftWork.data());
        for (int b = 0; b < 2048; ++b) {
            const float magDb = juce::Decibels::gainToDecibels(fftWork[static_cast<size_t>(b)] / 1024.0f, -96.0f);
            smoothedBins[static_cast<size_t>(b)] = smoothedBins[static_cast<size_t>(b)] * 0.72f + magDb * 0.28f;
        }
    }
    repaint();
}

void FidiuCompressorEditor::paintKneeOverlay(juce::Graphics& g, juce::Rectangle<float> box) {
    g.setColour(juce::Colour(0xd4151820));
    g.fillRoundedRectangle(box, 5.0f);
    g.setColour(juce::Colours::white.withAlpha(0.11f));
    g.drawRoundedRectangle(box, 5.0f, 1.0f);

    g.setColour(juce::Colour(0xffaab4c5));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("KNEE", juce::Rectangle<float>(box.getX() + 8.0f, box.getY() + 3.0f, 52.0f, 15.0f), juce::Justification::left, false);

    const float ratio = audioProcessor.apvts.getRawParameterValue("ratio")->load();
    g.setColour(kSilver);
    g.drawText(ratio >= 99.0f ? juce::String("inf:1") : juce::String(ratio, 1) + ":1",
               juce::Rectangle<float>(box.getRight() - 62.0f, box.getY() + 3.0f, 54.0f, 15.0f), juce::Justification::right, false);

    auto plot = box.reduced(14.0f, 18.0f).translated(0.0f, 5.0f);
    auto dbX = [&](float db) { return plot.getX() + ((db + 60.0f) / 60.0f) * plot.getWidth(); };
    auto dbY = [&](float db) { return plot.getY() + ((-db) / 60.0f) * plot.getHeight(); };

    for (float v : { -48.0f, -36.0f, -24.0f, -12.0f }) {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine(static_cast<int>(dbX(v)), plot.getY(), plot.getBottom());
        g.drawHorizontalLine(static_cast<int>(dbY(v)), plot.getX(), plot.getRight());
    }
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRect(plot, 1.0f);

    // 1:1 line
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawLine(plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);

    const auto styleIdx = static_cast<fidiu::CompressionStyle>(getChoiceParam("style"));
    const auto style = fidiu::getStyleProfile(styleIdx);
    const float thresh = audioProcessor.apvts.getRawParameterValue("threshold")->load();
    const float knee = audioProcessor.apvts.getRawParameterValue("knee")->load() * style.kneeMult;
    const float range = audioProcessor.apvts.getRawParameterValue("range")->load();
    const bool bypassed = audioProcessor.apvts.getRawParameterValue("bypass")->load() > 0.5f;

    juce::Path curve;
    juce::Path fill;
    for (int i = 0; i <= 60; ++i) {
        const float inDb = -60.0f + static_cast<float>(i);
        const float outDb = computeCurveOutDb(inDb, thresh, ratio, knee, range);
        const float px = dbX(inDb);
        const float py = dbY(outDb);
        if (i == 0) {
            curve.startNewSubPath(px, py);
            fill.startNewSubPath(px, py);
        } else {
            curve.lineTo(px, py);
            fill.lineTo(px, py);
        }
    }
    fill.lineTo(plot.getRight(), plot.getY());
    fill.lineTo(plot.getX(), plot.getBottom());
    fill.closeSubPath();

    if (!bypassed) {
        g.setColour(kOrange.withAlpha(0.18f));
        g.fillPath(fill);
    }

    g.setColour(bypassed ? juce::Colour(0xff64748b) : kOrange);
    g.strokePath(curve, juce::PathStrokeType(2.0f));

    // Threshold marker
    const float tx = dbX(thresh);
    const float ty = dbY(computeCurveOutDb(thresh, thresh, ratio, knee, range));
    g.setColour(kCyan.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(tx), plot.getY(), plot.getBottom());
    g.drawHorizontalLine(static_cast<int>(ty), plot.getX(), plot.getRight());
    g.setColour(juce::Colour(0xff14171e));
    g.fillEllipse(tx - 3.5f, ty - 3.5f, 7.0f, 7.0f);
    g.setColour(kCyan);
    g.drawEllipse(tx - 3.5f, ty - 3.5f, 7.0f, 7.0f, 1.5f);

    // Live bouncing ball
    const float maxIn = std::max(audioProcessor.currentInDbL.load(std::memory_order_relaxed),
                                 audioProcessor.currentInDbR.load(std::memory_order_relaxed));
    if (maxIn > -58.0f) {
        const float cIn = std::clamp(maxIn, -60.0f, 0.0f);
        const float cOut = bypassed ? cIn : computeCurveOutDb(cIn, thresh, ratio, knee, range);
        const float dx = dbX(cIn);
        const float dy = dbY(cOut);
        g.setColour(kCyan.withAlpha(0.28f));
        g.fillEllipse(dx - 6.0f, dy - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(dx - 2.8f, dy - 2.8f, 5.6f, 5.6f);
    }
}

void FidiuCompressorEditor::paintWaveformDisplay(juce::Graphics& g, juce::Rectangle<float> area) {
    auto plot = area.reduced(44.0f, 14.0f).withRight(area.getRight() - 14.0f);
    auto dbToY = [&](float db) {
        const float c = std::clamp(db, -60.0f, 0.0f);
        return plot.getY() + ((-c) / 60.0f) * plot.getHeight();
    };

    g.setFont(juce::FontOptions(12.0f));
    for (float db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f }) {
        const float y = dbToY(db);
        g.setColour(db == 0.0f ? juce::Colours::white.withAlpha(0.16f) : juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        g.setColour(juce::Colour(0xff8b96aa));
        g.drawText(juce::String(static_cast<int>(db)), static_cast<int>(area.getX() + 4), static_cast<int>(y - 8), 34, 16, juce::Justification::right, false);
    }

    const int n = FidiuCompressorProcessor::kHistSize;
    const int wIdx = audioProcessor.histWriteIdx.load(std::memory_order_relaxed);
    const float zeroY = dbToY(0.0f);
    const float floorY = dbToY(-60.0f);

    juce::Path inFill, outFill, outLine, grFill, grLine;
    inFill.startNewSubPath(plot.getX(), floorY);
    outFill.startNewSubPath(plot.getX(), floorY);
    grFill.startNewSubPath(plot.getX(), zeroY);

    for (int i = 0; i < n; ++i) {
        const int rIdx = (wIdx + i) % n;
        const float px = plot.getX() + (static_cast<float>(i) / static_cast<float>(n - 1)) * plot.getWidth();
        const float inY = dbToY(audioProcessor.histInDb[static_cast<size_t>(rIdx)].load(std::memory_order_relaxed));
        const float outY = dbToY(audioProcessor.histOutDb[static_cast<size_t>(rIdx)].load(std::memory_order_relaxed));
        const float grY = dbToY(-audioProcessor.histGrDb[static_cast<size_t>(rIdx)].load(std::memory_order_relaxed));

        inFill.lineTo(px, inY);
        outFill.lineTo(px, outY);
        grFill.lineTo(px, grY);
        if (i == 0) {
            outLine.startNewSubPath(px, outY);
            grLine.startNewSubPath(px, grY);
        } else {
            outLine.lineTo(px, outY);
            grLine.lineTo(px, grY);
        }
    }

    inFill.lineTo(plot.getRight(), floorY);
    inFill.closeSubPath();
    outFill.lineTo(plot.getRight(), floorY);
    outFill.closeSubPath();
    grFill.lineTo(plot.getRight(), zeroY);
    grFill.closeSubPath();

    // Input shadow
    g.setColour(juce::Colour(0x38697e9e));
    g.fillPath(inFill);

    // Output body
    juce::ColourGradient outGrad(juce::Colour(0x78e0e8f2), 0.0f, plot.getY(),
                                 juce::Colour(0x1a8294af), 0.0f, floorY, false);
    g.setGradientFill(outGrad);
    g.fillPath(outFill);
    g.setColour(juce::Colour(0xd0ebf1f8));
    g.strokePath(outLine, juce::PathStrokeType(1.4f));

    // Signature Crimson-Orange Gain Reduction Trace
    const bool bypassed = audioProcessor.apvts.getRawParameterValue("bypass")->load() > 0.5f;
    if (!bypassed) {
        juce::ColourGradient grGrad(kOrange.withAlpha(0.36f), 0.0f, zeroY,
                                    kOrange.withAlpha(0.02f), 0.0f, floorY, false);
        g.setGradientFill(grGrad);
        g.fillPath(grFill);

        g.setColour(kOrange.withAlpha(0.32f));
        g.strokePath(grLine, juce::PathStrokeType(4.5f));
        g.setColour(kOrange);
        g.strokePath(grLine, juce::PathStrokeType(2.0f));
    }

    // Draggable Threshold Line
    const float thresh = audioProcessor.apvts.getRawParameterValue("threshold")->load();
    const float ty = dbToY(thresh);
    g.setColour(dragTarget == DragTarget::Threshold ? juce::Colours::white : kCyan.withAlpha(0.75f));
    const float dashes[2] = { 4.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(plot.getX(), ty, plot.getRight(), ty), dashes, 2, 1.2f);

    juce::Rectangle<float> tag(plot.getRight() - 80.0f, ty - 10.0f, 80.0f, 20.0f);
    g.setColour(juce::Colour(0xe812161e));
    g.fillRoundedRectangle(tag, 3.0f);
    g.setColour(kCyan.withAlpha(0.8f));
    g.drawRoundedRectangle(tag, 3.0f, 1.0f);
    g.setColour(kCyan);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText(juce::String(thresh, 1) + " dB", tag.toNearestInt(), juce::Justification::centred, false);

    if (showKnee) {
        paintKneeOverlay(g, juce::Rectangle<float>(plot.getX() + 10.0f, plot.getBottom() - 190.0f, 180.0f, 180.0f));
    }
}

void FidiuCompressorEditor::paintSidechainDisplay(juce::Graphics& g, juce::Rectangle<float> area) {
    auto plot = area.reduced(44.0f, 20.0f).withRight(area.getRight() - 16.0f).withBottom(area.getBottom() - 26.0f);
    auto eqDbY = [&](float db) { return plot.getY() + ((24.0f - std::clamp(db, -36.0f, 24.0f)) / 60.0f) * plot.getHeight(); };

    g.setFont(juce::FontOptions(12.0f));
    for (float f : { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f }) {
        const float x = freqToX(f, plot);
        g.setColour(juce::Colours::white.withAlpha(0.055f));
        g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
        g.setColour(juce::Colour(0xff8b96aa));
        g.drawText(f >= 1000.0f ? juce::String(static_cast<int>(f / 1000.0f)) + "k" : juce::String(static_cast<int>(f)),
                   static_cast<int>(x - 18), static_cast<int>(plot.getBottom() + 4), 36, 16, juce::Justification::centred, false);
    }

    for (float db : { 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f }) {
        const float y = eqDbY(db);
        g.setColour(db == 0.0f ? juce::Colours::white.withAlpha(0.18f) : juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        g.setColour(juce::Colour(0xff8b96aa));
        g.drawText((db > 0.0f ? "+" : "") + juce::String(static_cast<int>(db)),
                   static_cast<int>(area.getX() + 4), static_cast<int>(y - 8), 34, 16, juce::Justification::right, false);
    }

    // 4096-pt Interpolated FFT Spectrum
    const double sr = audioProcessor.currentSampleRate.load(std::memory_order_relaxed);
    const float nyquist = static_cast<float>(sr * 0.5);
    juce::Path fftPath;
    fftPath.startNewSubPath(plot.getX(), plot.getBottom());
    for (int i = 0; i <= 260; ++i) {
        const float norm = static_cast<float>(i) / 260.0f;
        const float f = 20.0f * std::pow(1000.0f, norm);
        const float exactBin = std::clamp((f / nyquist) * 2048.0f, 1.0f, 2046.99f);
        const int b0 = static_cast<int>(exactBin);
        const int b1 = std::min(2047, b0 + 1);
        const float frac = exactBin - static_cast<float>(b0);
        const float interpDb = smoothedBins[static_cast<size_t>(b0)] * (1.0f - frac)
                             + smoothedBins[static_cast<size_t>(b1)] * frac;
        const float magDb = std::clamp(interpDb, -96.0f, 0.0f);
        const float yNorm = std::clamp(-magDb / 84.0f, 0.0f, 1.0f);
        fftPath.lineTo(freqToX(f, plot), plot.getY() + yNorm * plot.getHeight());
    }
    fftPath.lineTo(plot.getRight(), plot.getBottom());
    fftPath.closeSubPath();

    juce::ColourGradient fftGrad(kOrange.withAlpha(0.34f), 0.0f, plot.getY(),
                                 kCyan.withAlpha(0.06f), 0.0f, plot.getBottom(), false);
    g.setGradientFill(fftGrad);
    g.fillPath(fftPath);

    static constexpr int slopeLookup[4] = { 6, 12, 24, 48 };
    const bool hpfOn = audioProcessor.apvts.getRawParameterValue("scHpfOn")->load() > 0.5f;
    const float hpfF = audioProcessor.apvts.getRawParameterValue("scHpfFreq")->load();
    const int hpfS = slopeLookup[std::clamp(getChoiceParam("scHpfSlope"), 0, 3)];
    const bool lpfOn = audioProcessor.apvts.getRawParameterValue("scLpfOn")->load() > 0.5f;
    const float lpfF = audioProcessor.apvts.getRawParameterValue("scLpfFreq")->load();
    const int lpfS = slopeLookup[std::clamp(getChoiceParam("scLpfSlope"), 0, 3)];
    const bool eqOn = audioProcessor.apvts.getRawParameterValue("scEqOn")->load() > 0.5f;
    const float eqF = audioProcessor.apvts.getRawParameterValue("scEqFreq")->load();
    const float eqG = audioProcessor.apvts.getRawParameterValue("scEqGain")->load();
    const float eqQ = audioProcessor.apvts.getRawParameterValue("scEqQ")->load();

    const float zeroY = eqDbY(0.0f);
    juce::Path eqFill, eqLine;
    eqFill.startNewSubPath(plot.getX(), zeroY);
    for (int i = 0; i <= 180; ++i) {
        const float f = 20.0f * std::pow(1000.0f, static_cast<float>(i) / 180.0f);
        const float px = freqToX(f, plot);
        const float py = eqDbY(calcFilterDb(f, hpfOn, hpfF, hpfS, lpfOn, lpfF, lpfS, eqOn, eqF, eqG, eqQ));
        eqFill.lineTo(px, py);
        if (i == 0) eqLine.startNewSubPath(px, py);
        else eqLine.lineTo(px, py);
    }
    eqFill.lineTo(plot.getRight(), zeroY);
    eqFill.closeSubPath();
    g.setColour(kCyan.withAlpha(0.16f));
    g.fillPath(eqFill);
    g.setColour(kCyan);
    g.strokePath(eqLine, juce::PathStrokeType(2.2f));

    auto drawNode = [&](float nx, float ny, juce::Colour col, const juce::String& label, bool active) {
        g.setColour(active ? col : juce::Colour(0xff181c24));
        g.fillEllipse(nx - 6.0f, ny - 6.0f, 12.0f, 12.0f);
        g.setColour(col);
        g.drawEllipse(nx - 6.0f, ny - 6.0f, 12.0f, 12.0f, 2.0f);
        g.setColour(active ? juce::Colour(0xfff1f5f9) : juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(label, static_cast<int>(nx - 42), static_cast<int>(ny - 24), 84, 15, juce::Justification::centred, false);
    };
    const juce::String hpfTxt = "HPF " + (hpfF >= 1000.0f ? juce::String(hpfF / 1000.0f, 1) + "k" : juce::String(static_cast<int>(std::round(hpfF))) + "Hz");
    const juce::String lpfTxt = "LPF " + (lpfF >= 1000.0f ? juce::String(lpfF / 1000.0f, 1) + "k" : juce::String(static_cast<int>(std::round(lpfF))) + "Hz");
    const juce::String eqTxt  = "EQ " + (eqG >= 0.0f ? juce::String("+") : juce::String("")) + juce::String(eqG, 1) + "dB";
    drawNode(freqToX(hpfF, plot), eqDbY(-3.0f), hpfOn ? kCyan : juce::Colour(0xff475569), hpfTxt, hpfOn);
    drawNode(freqToX(lpfF, plot), eqDbY(-3.0f), lpfOn ? kCyan : juce::Colour(0xff475569), lpfTxt, lpfOn);
    drawNode(freqToX(eqF, plot), eqDbY(eqG), eqOn ? kAmber : juce::Colour(0xff475569), eqTxt, eqOn);
}

void FidiuCompressorEditor::paintMeters(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(juce::Colour(0xff11141a));
    g.fillRect(area);
    g.setColour(juce::Colour(0xff282d38));
    g.drawVerticalLine(static_cast<int>(area.getX()), area.getY(), area.getBottom());

    auto dbPct = [](float db) { return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f); };
    const float inL = dbPct(audioProcessor.currentInDbL.load(std::memory_order_relaxed));
    const float inR = dbPct(audioProcessor.currentInDbR.load(std::memory_order_relaxed));
    const float outL = dbPct(audioProcessor.currentOutDbL.load(std::memory_order_relaxed));
    const float outR = dbPct(audioProcessor.currentOutDbR.load(std::memory_order_relaxed));
    const bool bypassed = audioProcessor.apvts.getRawParameterValue("bypass")->load() > 0.5f;
    const float grPct = bypassed ? 0.0f : std::clamp(audioProcessor.currentGrDb.load(std::memory_order_relaxed) / 36.0f, 0.0f, 1.0f);

    const float pIn = audioProcessor.peakHoldInDb.load(std::memory_order_relaxed);
    const float pOut = audioProcessor.peakHoldOutDb.load(std::memory_order_relaxed);
    const float pGr = bypassed ? 0.0f : audioProcessor.peakHoldGrDb.load(std::memory_order_relaxed);

    const float topY = area.getY() + 26.0f;
    const float botY = area.getBottom() - 24.0f;
    const float barH = std::max(40.0f, botY - topY);

    // Scale ticks
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(juce::Colour(0xff8b96aa));
    const float ticks[5] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f };
    for (int i = 0; i < 5; ++i) {
        const float ty = topY + (static_cast<float>(i) / 4.0f) * (barH - 12.0f);
        g.drawText(juce::String(static_cast<int>(ticks[i])), static_cast<int>(area.getX() + 2), static_cast<int>(ty), 26, 12, juce::Justification::right, false);
    }

    auto drawBottomUpBar = [&](float bx, float bw, float pct, juce::Colour topCol, juce::Colour botCol) {
        g.setColour(juce::Colour(0xff0a0c10));
        g.fillRect(bx, topY, bw, barH);
        const float fh = barH * pct;
        if (fh > 0.5f) {
            juce::ColourGradient grad(topCol, bx, topY, botCol, bx, botY, false);
            g.setGradientFill(grad);
            g.fillRect(bx, botY - fh, bw, fh);
        }
    };

    const float xBase = area.getX() + 38.0f;
    // IN stereo
    drawBottomUpBar(xBase, 6.0f, inL, juce::Colour(0xff9db0c9), juce::Colour(0xff364152));
    drawBottomUpBar(xBase + 8.0f, 6.0f, inR, juce::Colour(0xff9db0c9), juce::Colour(0xff364152));
    // GR top-down
    const float grX = xBase + 38.0f;
    g.setColour(juce::Colour(0xff0a0c10));
    g.fillRect(grX, topY, 10.0f, barH);
    if (grPct > 0.005f) {
        juce::ColourGradient grGrad(kOrange, grX, topY, kAmber, grX, botY, false);
        g.setGradientFill(grGrad);
        g.fillRect(grX, topY, 10.0f, barH * grPct);
    }
    // OUT stereo
    const float outX = grX + 34.0f;
    drawBottomUpBar(outX, 6.0f, outL, kCyan.brighter(0.35f), juce::Colour(0xff186480));
    drawBottomUpBar(outX + 8.0f, 6.0f, outR, kCyan.brighter(0.35f), juce::Colour(0xff186480));

    // Numeric readouts (generous widths, bold, zero ellipsis truncation)
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.setColour(juce::Colour(0xffcbd5e1));
    g.drawText(pIn <= -59.0f ? juce::String("-inf") : juce::String(pIn, 1), static_cast<int>(xBase - 11), static_cast<int>(area.getY() + 5), 36, 16, juce::Justification::centred, false);

    g.setColour(kOrange);
    g.drawText(pGr < 0.05f ? juce::String("0.0") : juce::String("-") + juce::String(pGr, 1), static_cast<int>(grX - 13), static_cast<int>(area.getY() + 5), 36, 16, juce::Justification::centred, false);

    g.setColour(juce::Colour(0xffe2e8f0));
    g.drawText(pOut <= -59.0f ? juce::String("-inf") : juce::String(pOut, 1), static_cast<int>(outX - 11), static_cast<int>(area.getY() + 5), 36, 16, juce::Justification::centred, false);

    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(juce::Colour(0xff8b96aa));
    g.drawText("IN", static_cast<int>(xBase - 6), static_cast<int>(botY + 4), 26, 16, juce::Justification::centred, false);
    g.setColour(kOrange);
    g.drawText("GR", static_cast<int>(grX - 8), static_cast<int>(botY + 4), 26, 16, juce::Justification::centred, false);
    g.setColour(kCyan);
    g.drawText("OUT", static_cast<int>(outX - 8), static_cast<int>(botY + 4), 30, 16, juce::Justification::centred, false);
}

void FidiuCompressorEditor::paint(juce::Graphics& g) {
    g.fillAll(kBgOuter);

    // Top Toolbar
    g.setColour(kBgTopBar);
    g.fillRect(0, 0, getWidth(), 42);
    g.setColour(kBorder);
    g.drawHorizontalLine(41, 0.0f, static_cast<float>(getWidth()));

    // Pro-C icon + Fidiu Compressor title
    g.setColour(juce::Colour(0xff13161d));
    g.fillRoundedRectangle(12.0f, 10.0f, 22.0f, 22.0f, 4.0f);
    g.setColour(kOrange);
    g.drawLine(15.0f, 27.0f, 23.0f, 19.0f, 2.0f);
    g.drawLine(23.0f, 19.0f, 31.0f, 15.0f, 2.0f);
    g.setColour(kCyan);
    g.fillEllipse(21.5f, 17.5f, 3.5f, 3.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Fidiu", 42, 10, 44, 22, juce::Justification::left, false);
    g.setColour(kMutedText);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Compressor", 86, 11, 86, 22, juce::Justification::left, false);

    juce::Rectangle<float> verBadge(174.0f, 12.0f, 48.0f, 18.0f);
    g.setColour(kCyan.withAlpha(0.12f));
    g.fillRoundedRectangle(verBadge, 3.0f);
    g.setColour(kCyan.withAlpha(0.38f));
    g.drawRoundedRectangle(verBadge, 3.0f, 1.0f);
    g.setColour(kCyan);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("V1.0.2", verBadge.toNearestInt(), juce::Justification::centred, false);

    // Display Well radial vignette
    auto dispF = displayBounds.toFloat();
    juce::ColourGradient vig(juce::Colour(0xff181c24), dispF.getCentreX(), dispF.getCentreY(),
                             juce::Colour(0xff101319), dispF.getX(), dispF.getY(), true);
    g.setGradientFill(vig);
    g.fillRect(dispF);

    if (displayMode == DisplayMode::Dynamics)
        paintWaveformDisplay(g, dispF);
    else
        paintSidechainDisplay(g, dispF);

    // Top-right HUD in display
    const bool laOn = audioProcessor.apvts.getRawParameterValue("lookaheadOn")->load() > 0.5f;
    const float laMs = audioProcessor.apvts.getRawParameterValue("lookaheadMs")->load();
    const float curGr = audioProcessor.currentGrDb.load(std::memory_order_relaxed);
    if (laOn && laMs > 0.05f) {
        g.setColour(kCyan);
        g.setFont(juce::FontOptions(12.5f));
        g.drawText("LA " + juce::String(laMs, 1) + " ms", displayBounds.getRight() - 216, displayBounds.getY() + 8, 92, 18, juce::Justification::right, false);
    }
    g.setColour(curGr > 0.2f ? kOrange : juce::Colour(0xff7d8799));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("GR -" + juce::String(curGr, 1) + " dB", displayBounds.getRight() - 116, displayBounds.getY() + 8, 104, 18, juce::Justification::right, false);

    paintMeters(g, metersBounds.toFloat());

    // Console Gradient
    juce::ColourGradient conGrad(kBgTopBar, 0.0f, static_cast<float>(consoleBounds.getY()),
                                 kBgConsole, 0.0f, static_cast<float>(consoleBounds.getBottom()), false);
    g.setGradientFill(conGrad);
    g.fillRect(consoleBounds);
    g.setColour(kBorder);
    g.drawHorizontalLine(consoleBounds.getY(), 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(consoleBounds.getY() + 38, 14.0f, static_cast<float>(getWidth() - 14));

    // Style / Coloriture labels
    g.setColour(juce::Colour(0xff7d8799));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("STYLE", 16, consoleBounds.getY() + 10, 44, 20, juce::Justification::left);
    g.drawText("COLORITURE", getWidth() - 322, consoleBounds.getY() + 10, 76, 20, juce::Justification::right);

    // Lower Lookahead & Sidechain Rail
    g.setColour(kBgRail);
    g.fillRect(scRailBounds);
    g.setColour(kBorder);
    g.drawHorizontalLine(scRailBounds.getY(), 0.0f, static_cast<float>(getWidth()));

    // Knob labels above each rotary slider
    g.setColour(kMutedText);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    for (const auto& kv : knobLabels) {
        auto b = kv.first->getBounds();
        g.drawText(kv.second, b.getX() - 4, b.getY() - 14, b.getWidth() + 8, 13, juce::Justification::centred, false);
    }
}

void FidiuCompressorEditor::resized() {
    const int w = getWidth();
    const int h = getHeight();

    // Top Toolbar (Preset + A/B + Copy + Reset + Level Match + Bypass)
    presetBox.setBounds(230, 8, 184, 26);
    bankABtn.setBounds(422, 8, 32, 26);
    bankBBtn.setBounds(455, 8, 32, 26);
    copyAbBtn.setBounds(493, 8, 56, 26);
    resetBtn.setBounds(555, 8, 52, 26);

    levelMatchBtn.setBounds(w - 196, 8, 96, 26);
    bypassBtn.setBounds(w - 92, 8, 78, 26);

    const int topH = 42;
    const int railH = 114;
    const int conH = 196;
    const int dispH = std::max(210, h - topH - conH - railH);

    const int meterW = 144;
    displayBounds = { 0, topH, w - meterW, dispH };
    metersBounds  = { w - meterW, topH, meterW, dispH };
    consoleBounds = { 0, topH + dispH, w, conH };
    scRailBounds  = { 0, topH + dispH + conH, w, railH };

    // Floating Display Mode Buttons
    dispModeBtn.setBounds(50, topH + 8, 66, 22);
    scModeBtn.setBounds(118, topH + 8, 98, 22);
    kneeToggleBtn.setBounds(220, topH + 8, 52, 22);

    // Style & Coloriture Sub-Bar
    const int barY = consoleBounds.getY() + 8;
    int sx = 64;
    for (int i = 0; i < 8; ++i) {
        styleBtns[static_cast<size_t>(i)].setBounds(sx, barY, 66, 22);
        sx += 69;
    }
    int cx = w - 240;
    for (int i = 0; i < 4; ++i) {
        colorBtns[static_cast<size_t>(i)].setBounds(cx, barY, 54, 22);
        cx += 57;
    }

    // Main Pro-C Knob Console (11 controls including Parallel Mix & Dry Gain)
    const int knobY = consoleBounds.getY() + 62;
    const int colW = (w - 32) / 11;
    threshSlider.setBounds(16, knobY - 6, colW, 108);
    ratioSlider.setBounds(16 + colW * 1, knobY, colW, 96);
    kneeSlider.setBounds(16 + colW * 2, knobY + 4, colW, 88);
    rangeSlider.setBounds(16 + colW * 3, knobY + 4, colW, 88);
    attackSlider.setBounds(16 + colW * 4, knobY + 4, colW, 88);
    holdSlider.setBounds(16 + colW * 5, knobY + 4, colW, 88);
    releaseSlider.setBounds(16 + colW * 6, knobY + 4, colW, 88);
    autoRelBtn.setBounds(16 + colW * 6 + colW / 2 - 26, knobY + 94, 52, 18);
    charSlider.setBounds(16 + colW * 7, knobY + 4, colW, 88);
    mixSlider.setBounds(16 + colW * 8, knobY + 4, colW, 88);
    parallelBtn.setBounds(16 + colW * 8 + colW / 2 - 30, knobY + 94, 60, 18);
    gainSlider.setBounds(16 + colW * 9, knobY + 4, colW, 88);
    autoGainBtn.setBounds(16 + colW * 9 + colW / 2 - 36, knobY + 94, 72, 18);
    dryGainSlider.setBounds(16 + colW * 10, knobY + 4, colW, 88);

    // Integrated Lower Lookahead, Mid/Side/Stereo & Sidechain Rail
    const int ry = scRailBounds.getY() + 18;
    const int extraGap = std::clamp((w - 980) / 5, 0, 42);
    int rx = 14;
    lookaheadBtn.setBounds(rx, ry + 2, 72, 20);
    for (int i = 0; i < 3; ++i)
        osBtns[static_cast<size_t>(i)].setBounds(rx + i * 24, ry + 28, 23, 18);
    rx += 76;
    lookaheadSlider.setBounds(rx, ry, 54, 74); rx += 56;
    linkSlider.setBounds(rx, ry, 52, 74); rx += 54;
    msSlider.setBounds(rx, ry, 52, 74); rx += 56;

    // Stereo / Mid / Side mode selector
    for (int i = 0; i < 3; ++i)
        chModeBtns[static_cast<size_t>(i)].setBounds(rx, ry + i * 22, 54, 19);
    rx += 60 + extraGap;

    // Sidechain routing
    scIntBtn.setBounds(rx, ry + 2, 47, 20);
    scExtBtn.setBounds(rx + 49, ry + 2, 47, 20);
    scAudBtn.setBounds(rx, ry + 28, 96, 20);
    rx += 102 + extraGap;

    // HPF (23px each for "6", "12", "24", "48")
    scHpfBtn.setBounds(rx, ry + 2, 95, 20);
    for (int i = 0; i < 4; ++i)
        hpfSlopeBtns[static_cast<size_t>(i)].setBounds(rx + i * 24, ry + 28, 23, 19);
    rx += 100;
    scHpfSlider.setBounds(rx, ry, 54, 74); rx += 58 + extraGap;

    // LPF (23px each for "6", "12", "24", "48")
    scLpfBtn.setBounds(rx, ry + 2, 95, 20);
    for (int i = 0; i < 4; ++i)
        lpfSlopeBtns[static_cast<size_t>(i)].setBounds(rx + i * 24, ry + 28, 23, 19);
    rx += 100;
    scLpfSlider.setBounds(rx, ry, 54, 74); rx += 58 + extraGap;

    // SC Bell EQ
    scEqBtn.setBounds(rx, ry + 14, 54, 22); rx += 58;
    scEqFreqSlider.setBounds(rx, ry, 52, 74); rx += 54;
    scEqGainSlider.setBounds(rx, ry, 52, 74); rx += 54;
    scEqQSlider.setBounds(rx, ry, 52, 74);
}

void FidiuCompressorEditor::mouseDown(const juce::MouseEvent& e) {
    if (metersBounds.contains(e.getPosition())) {
        audioProcessor.resetPeaks();
        return;
    }
    if (!displayBounds.contains(e.getPosition())) return;

    auto area = displayBounds.toFloat();
    if (displayMode == DisplayMode::Dynamics) {
        auto plot = area.reduced(44.0f, 14.0f).withRight(area.getRight() - 14.0f);
        const float thresh = audioProcessor.apvts.getRawParameterValue("threshold")->load();
        const float ty = plot.getY() + ((-thresh) / 60.0f) * plot.getHeight();
        if (std::abs(e.position.y - ty) < 16.0f)
            dragTarget = DragTarget::Threshold;
    } else {
        auto plot = area.reduced(44.0f, 20.0f).withRight(area.getRight() - 16.0f).withBottom(area.getBottom() - 26.0f);
        auto eqDbY = [&](float db) { return plot.getY() + ((24.0f - std::clamp(db, -36.0f, 24.0f)) / 60.0f) * plot.getHeight(); };
        const float hpfX = freqToX(audioProcessor.apvts.getRawParameterValue("scHpfFreq")->load(), plot);
        const float lpfX = freqToX(audioProcessor.apvts.getRawParameterValue("scLpfFreq")->load(), plot);
        const float eqX = freqToX(audioProcessor.apvts.getRawParameterValue("scEqFreq")->load(), plot);
        const float eqY = eqDbY(audioProcessor.apvts.getRawParameterValue("scEqGain")->load());
        const float baseY = eqDbY(-3.0f);

        if (e.position.getDistanceFrom({ eqX, eqY }) < 18.0f) dragTarget = DragTarget::Eq;
        else if (e.position.getDistanceFrom({ hpfX, baseY }) < 18.0f) dragTarget = DragTarget::Hpf;
        else if (e.position.getDistanceFrom({ lpfX, baseY }) < 18.0f) dragTarget = DragTarget::Lpf;
    }
}

void FidiuCompressorEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragTarget == DragTarget::None) return;
    auto area = displayBounds.toFloat();
    auto setVal = [this](const juce::String& id, float v) {
        if (auto* param = audioProcessor.apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(v));
    };

    if (dragTarget == DragTarget::Threshold) {
        auto plot = area.reduced(44.0f, 14.0f).withRight(area.getRight() - 14.0f);
        const float n = std::clamp((e.position.y - plot.getY()) / std::max(1.0f, plot.getHeight()), 0.0f, 1.0f);
        setVal("threshold", -60.0f * n);
    } else {
        auto plot = area.reduced(44.0f, 20.0f).withRight(area.getRight() - 16.0f).withBottom(area.getBottom() - 26.0f);
        const float freq = xToFreq(e.position.x, plot);
        if (dragTarget == DragTarget::Hpf) {
            setVal("scHpfOn", 1.0f);
            setVal("scHpfFreq", freq);
        } else if (dragTarget == DragTarget::Lpf) {
            setVal("scLpfOn", 1.0f);
            setVal("scLpfFreq", freq);
        } else if (dragTarget == DragTarget::Eq) {
            const float n = std::clamp((e.position.y - plot.getY()) / std::max(1.0f, plot.getHeight()), 0.0f, 1.0f);
            const float gain = std::clamp(24.0f - n * 60.0f, -18.0f, 18.0f);
            setVal("scEqOn", 1.0f);
            setVal("scEqFreq", std::clamp(freq, 40.0f, 16000.0f));
            setVal("scEqGain", gain);
        }
    }
}

void FidiuCompressorEditor::mouseUp(const juce::MouseEvent&) {
    dragTarget = DragTarget::None;
}
