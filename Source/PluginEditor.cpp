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

juce::Rectangle<int> FidiuLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) {
    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    const int sep = tipText.indexOf("\n");
    if (sep > 0) {
        s.append(tipText.substring(0, sep) + "\n", juce::FontOptions(12.0f, juce::Font::bold), kCyan);
        s.append(tipText.substring(sep + 1), juce::FontOptions(11.5f), juce::Colour(0xffdce3ee));
    } else {
        s.append(tipText, juce::FontOptions(11.5f), juce::Colour(0xffdce3ee));
    }
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, 280.0f);
    const int w = static_cast<int>(std::ceil(tl.getWidth())) + 24;
    const int h = static_cast<int>(std::ceil(tl.getHeight())) + 20;
    const int x = juce::jlimit(parentArea.getX() + 6, std::max(parentArea.getX() + 6, parentArea.getRight() - w - 6), screenPos.x - w / 2);
    int y = screenPos.y - h - 14;
    if (y < parentArea.getY() + 6)
        y = std::min(parentArea.getBottom() - h - 6, screenPos.y + 18);
    return { x, y, w, h };
}

void FidiuLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.5f);
    g.setColour(juce::Colour(0xf511151e));
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(kCyan.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds, 7.0f, 1.2f);

    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    const int sep = text.indexOf("\n");
    if (sep > 0) {
        s.append(text.substring(0, sep) + "\n", juce::FontOptions(12.0f, juce::Font::bold), kCyan);
        s.append(text.substring(sep + 1), juce::FontOptions(11.5f), juce::Colour(0xffdce3ee));
    } else {
        s.append(text, juce::FontOptions(11.5f), juce::Colour(0xffdce3ee));
    }
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths(s, static_cast<float>(std::max(80, width - 22)));
    tl.draw(g, bounds.reduced(10.0f, 8.0f));
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

    static const char* presetNames[20] = {
        "Bus: Studio Bus Glue",
        "Mastering: Transparent",
        "Vocal: Forward Opto",
        "Drums: Parallel Punch",
        "Limiting: Zero-Overshoot",
        "Electronic: Kick Ducking",
        "Bus: Warm Tape Mix Bus",
        "Bus: Side Width Control",
        "Mastering: Loud & Clear",
        "Mastering: Mid Analog Sheen",
        "Kick: Sub Weight & Snap",
        "Kick: Tight Electronic Click",
        "Bass: Sub Lock Opto",
        "Bass: Parallel Growl",
        "Hats: Silky De-Harsh",
        "Guitar: Acoustic Body",
        "Guitar: Crunch Tightener",
        "Piano: Concert Grand",
        "Synths: Lush Wide Pad",
        "Synths: Parallel Arp Slap"
    };
    for (int i = 0; i < 20; ++i)
        presetBox.addItem(presetNames[i], i + 1);
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

    styleBtn(presetPrevBtn, juce::Colour(0xff2c3342));
    styleBtn(presetNextBtn, juce::Colour(0xff2c3342));
    presetPrevBtn.setClickingTogglesState(false);
    presetNextBtn.setClickingTogglesState(false);
    presetPrevBtn.onClick = [this] { stepPreset(-1); };
    presetNextBtn.onClick = [this] { stepPreset(1); };

    styleBtn(bankABtn, juce::Colour(0xff164e63));
    styleBtn(bankBBtn, kOrange.darker(0.2f));
    styleBtn(copyAbBtn, juce::Colour(0xff2c3342));
    styleBtn(resetBtn, juce::Colour(0xff2c3342));
    styleBtn(analyzeBtn, juce::Colour(0xff164e63));
    styleBtn(langBtn, juce::Colour(0xff164e63));
    styleBtn(levelMatchBtn, juce::Colour(0xff164e63));
    styleBtn(bypassBtn, kAmber.darker(0.35f));
    styleBtn(dispModeBtn, juce::Colour(0xff2c3342));
    styleBtn(scModeBtn, juce::Colour(0xff164e63));
    styleBtn(kneeToggleBtn, juce::Colour(0xff2c3342));

    bankABtn.setClickingTogglesState(false);
    bankBBtn.setClickingTogglesState(false);
    copyAbBtn.setClickingTogglesState(false);
    resetBtn.setClickingTogglesState(false);
    analyzeBtn.setClickingTogglesState(false);
    langBtn.setClickingTogglesState(false);

    bankABtn.onClick = [this] { audioProcessor.selectBank(0); updateButtonStates(); repaint(); };
    bankBBtn.onClick = [this] { audioProcessor.selectBank(1); updateButtonStates(); repaint(); };
    copyAbBtn.onClick = [this] {
        audioProcessor.copyActiveBankToOther();
        copyAbBtn.setButtonText(audioProcessor.getActiveBank() == 0 ? "A -> B" : "B -> A");
        copyFlashFrames = 55;
    };
    resetBtn.onClick = [this] { audioProcessor.resetToDefault(); updateButtonStates(); repaint(); };
    analyzeBtn.onClick = [this] { startSignalAnalysis(); };
    langBtn.onClick = [this] {
        const bool isRo = audioProcessor.uiLanguage.load(std::memory_order_relaxed) == 1;
        juce::PopupMenu menu;
        menu.addItem(1, "English", true, !isRo);
        menu.addItem(2, juce::String::fromUTF8("Română"), true, isRo);
        juce::Component::SafePointer<FidiuCompressorEditor> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&langBtn),
            [safeThis](int result) {
                if (safeThis == nullptr) return;
                if (result == 1 || result == 2) {
                    safeThis->audioProcessor.uiLanguage.store(result - 1, std::memory_order_relaxed);
                    safeThis->applyTooltipLanguage();
                    safeThis->repaint();
                }
            });
    };

    for (size_t i = 0; i < 3; ++i) {
        styleBtn(sugPresetBtns[i], i == 0 ? juce::Colour(0xff164e63) : juce::Colour(0xff2c3342));
        sugPresetBtns[i].setClickingTogglesState(false);
        sugPresetBtns[i].setVisible(false);
        sugPresetBtns[i].onClick = [this, i] {
            const int pIdx = std::clamp(sugPresetIndices[i], 0, 19);
            presetBox.setSelectedId(pIdx + 1, juce::dontSendNotification);
            applyPreset(pIdx);
            updateButtonStates();
            repaint();
        };
    }
    styleBtn(sugCloseBtn, juce::Colour(0xff2c3342));
    sugCloseBtn.setClickingTogglesState(false);
    sugCloseBtn.setVisible(false);
    sugCloseBtn.onClick = [this] {
        hasSuggestion = false;
        for (auto& b : sugPresetBtns) b.setVisible(false);
        sugCloseBtn.setVisible(false);
        repaint();
    };

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
    applyTooltipLanguage();

    startTimerHz(60);
}

FidiuCompressorEditor::~FidiuCompressorEditor() {
    setLookAndFeel(nullptr);
}

void FidiuCompressorEditor::applyTooltipLanguage() {
    const int lang = std::clamp(audioProcessor.uiLanguage.load(std::memory_order_relaxed), 0, 1);
    const bool isRo = (lang == 1);
    lastAppliedLang = lang;

    auto u8 = [](const char* s) { return juce::String::fromUTF8(s); };
    auto tr = [&](const char* en, const char* ro) { return u8(isRo ? ro : en); };

    langBtn.setButtonText(isRo ? "RO v" : "EN v");
    langBtn.setTooltip(tr(
        "Tooltip Language Menu\nSwitch control descriptions and tooltip balloons between English and Romanian (Română).",
        "Meniu Limbă Descrieri\nComută descrierile tuturor controalelor între Limba Română și Engleză."
    ));

    presetPrevBtn.setTooltip(tr(
        "Previous Preset\nSteps backward through the 20 factory presets.",
        "Preset Anterior\nTrece la presetul anterior din cele 20 de preseturi integrate."
    ));
    presetBox.setTooltip(tr(
        "Factory Presets\nSelect from 20 studio presets for Mix Bus, Mastering, Vocals, Drums, Kick, Bass, Hats, Guitar, Piano, and Synths.",
        "Preseturi de Fabrică\nAlege din 20 de preseturi de studio pentru Mix Bus, Mastering, Voce, Tobe, Kick, Bass, Hats, Chitară, Pian și Synths."
    ));
    presetNextBtn.setTooltip(tr(
        "Next Preset\nSteps forward to the next factory preset.",
        "Preset Următor\nAvansează la următorul preset din listă."
    ));
    bankABtn.setTooltip(tr(
        "Compare Bank A\nSwitches to parameter slot A for instant A/B comparison.",
        "Bancă Comparație A\nActivează slotul A de parametri pentru comparație rapidă A/B."
    ));
    bankBBtn.setTooltip(tr(
        "Compare Bank B\nSwitches to parameter slot B for instant A/B comparison.",
        "Bancă Comparație B\nActivează slotul B de parametri pentru comparație rapidă A/B."
    ));
    copyAbBtn.setTooltip(tr(
        "Copy A/B Slot\nCopies settings from the active slot (A or B) into the opposite slot.",
        "Copiere Slot A/B\nCopiază toate setările din slotul activ (A sau B) în celălalt slot pentru variații rapide."
    ));
    resetBtn.setTooltip(tr(
        "Reset Parameters\nRestores all compressor, Coloriture, and sidechain controls to defaults.",
        "Resetare Parametri\nReaduce compresorul, Coloriture și sidechain-ul la valorile implicite."
    ));
    analyzeBtn.setTooltip(tr(
        "Analyze Input Signal\nScans frequency balance, crest factor, and Mid/Side width to identify the signal and suggest matching presets.",
        "Analizare Semnal Intrare\nAnalizează spectrul, factorul crest și lățimea Mid/Side pentru a detecta instrumentul sau mix-busul și a sugera preseturi."
    ));
    levelMatchBtn.setTooltip(tr(
        "Auto Level Match\nMatches compressed output RMS loudness to input loudness for fair Bypass comparison.",
        "Potrivire Automată Nivel\nEgalizează volumul RMS al ieșirii comprimate cu cel al intrării pentru comparații corecte la Bypass."
    ));
    bypassBtn.setTooltip(tr(
        "Bypass Processor\nCleanly bypasses dynamics, saturation, and makeup gain for reference comparison.",
        "Bypass Procesor\nDezactivează temporar compresia, saturația și amplificarea pentru comparație directă cu semnalul neprocesat."
    ));
    dispModeBtn.setTooltip(tr(
        "Dynamics Display\nShows scrolling Input/Output envelopes and Gain Reduction trace. Drag cyan line to set Threshold.",
        "Afișaj Dinamică\nAfișează anvelopele Input/Output și curba de reducere a câștigului (GR). Trage linia cyan pentru Threshold."
    ));
    scModeBtn.setTooltip(tr(
        "Sidechain EQ & 4096-pt FFT\nShows high-resolution sidechain FFT spectrum and interactive filter curve. Scroll wheel adjusts Q or slope.",
        "Sidechain EQ & FFT 4096-pt\nAfișează spectrul FFT de înaltă rezoluție și curba EQ sidechain. Rotița mouse-ului reglează Q sau panta."
    ));
    kneeToggleBtn.setTooltip(tr(
        "Transfer Knee Curve\nToggles the live input-vs-output transfer curve overlay.",
        "Curbă Transfer Knee\nActivează sau ascunde graficul live al curbei de transfer Input/Output."
    ));

    static const char* styleTipsEn[8] = {
        "Style: Clean\nTransparent digital dynamics with very low harmonic coloration.",
        "Style: Classic\nBalanced VCA-style punch with subtle analog character.",
        "Style: Opto\nSmooth slow-release optical curve ideal for vocals, bass, and acoustic sources.",
        "Style: Vocal\nAdaptive presence detection keeping vocals forward and even.",
        "Style: Mastering\nUltra-clean RMS-weighted transparent glue for stereo mastering.",
        "Style: Bus\nCohesive drum and mix-bus glue with musical dual-stage release.",
        "Style: Punch\nFast transient snap and aggressive attack bite for drums and percussive tracks.",
        "Style: Pumping\nFast recovery and tight detector timing for classic EDM pumping."
    };
    static const char* styleTipsRo[8] = {
        "Stil: Clean\nDinamică digitală ultra-transparentă, cu distorsiuni armonice minime.",
        "Stil: Classic\nImpact și densitate clasică VCA dependentă de program, cu caracter analogic subtil.",
        "Stil: Opto\nRevenire optică lentă în două etape, ideală pentru voce, bass și instrumente acustice.",
        "Stil: Vocal\nControl dinamic adaptiv ce menține vocea clară și prezentă în față în mix.",
        "Stil: Mastering\nCompresie coerentă de înaltă precizie RMS, optimizată pentru mastering stereo transparent.",
        "Stil: Bus\nCoeziune muzicală pentru mix bus și grupuri, ce leagă tobele, bassul și instrumentele.",
        "Stil: Punch\nAtac rapid și contur percusiv agresiv pentru tobe și tranziții accentuate.",
        "Stil: Pumping\nRevenire rapidă și respirație ritmică ideală pentru efectul clasic de pumping EDM."
    };
    for (int i = 0; i < 8; ++i)
        styleBtns[static_cast<size_t>(i)].setTooltip(tr(styleTipsEn[i], styleTipsRo[i]));

    static const char* colorTipsEn[4] = {
        "Coloriture: Pure\nClean symmetrical saturation for minimal harmonic distortion.",
        "Coloriture: Tube\nAsymmetric triode warmth adding rich even-order tube harmonics.",
        "Coloriture: Tape\nSmooth magnetic tape saturation with gentle top-end sheen.",
        "Coloriture: Xfmr\nIron transformer saturation adding dense low-mid weight and punch."
    };
    static const char* colorTipsRo[4] = {
        "Coloriture: Pure\nSaturație simetrică extrem de curată pentru control digital transparent.",
        "Coloriture: Tube\nCăldură asimetrică de lampă (triodă) ce adaugă armonice pare plăcute.",
        "Coloriture: Tape\nSaturație fină de bandă magnetică cu rotunjire plăcută a înaltelor.",
        "Coloriture: Xfmr\nSaturație de transformator pe fier ce adaugă greutate în joase-medii și punch."
    };
    for (int i = 0; i < 4; ++i)
        colorBtns[static_cast<size_t>(i)].setTooltip(tr(colorTipsEn[i], colorTipsRo[i]));

    static const char* osTipsEn[3] = {
        "Oversampling 1x\nZero-oversampling standard sample rate processing.",
        "Oversampling 2x\nDoubles internal DSP rate to reduce aliasing on fast transients and Drive.",
        "Oversampling 4x\nQuadruples internal DSP rate for pristine limiting and analog saturation."
    };
    static const char* osTipsRo[3] = {
        "Oversampling 1x\nProcesare standard la frecvența de eșantionare a proiectului.",
        "Oversampling 2x\nDublează rata internă DSP pentru reducerea aliasing-ului la Drive și atacuri rapide.",
        "Oversampling 4x\nCvadruplează rata internă pentru limitare curată și saturație analogică impecabilă."
    };
    for (int i = 0; i < 3; ++i)
        osBtns[static_cast<size_t>(i)].setTooltip(tr(osTipsEn[i], osTipsRo[i]));

    static const char* chModeTipsEn[3] = {
        "Routing: Stereo\nProcesses full Left/Right stereo audio with optional M/S Balance bias.",
        "Routing: Mid\nCompresses only the center (Mid) mono signal, leaving stereo Sides untouched.",
        "Routing: Side\nCompresses only the stereo Side width without touching center punch."
    };
    static const char* chModeTipsRo[3] = {
        "Procesare: Stereo\nProcesează întregul semnal stereo Left/Right, cu ajustare opțională M/S Balance.",
        "Procesare: Mid\nComprimă exclusiv canalul central (Mid), lăsând lateralele stereo (Side) neatinse.",
        "Procesare: Side\nComprimă exclusiv informația laterală (Side) fără a afecta impactul din centru."
    };
    for (int i = 0; i < 3; ++i)
        chModeBtns[static_cast<size_t>(i)].setTooltip(tr(chModeTipsEn[i], chModeTipsRo[i]));

    static const char* slopeNames[4] = { "6", "12", "24", "48" };
    for (int i = 0; i < 4; ++i) {
        if (isRo) {
            hpfSlopeBtns[static_cast<size_t>(i)].setTooltip(u8("Pantă HPF (") + slopeNames[i] + u8(" dB/oct)\nSetează abruptimea filtrului trece-sus din detectorul sidechain."));
            lpfSlopeBtns[static_cast<size_t>(i)].setTooltip(u8("Pantă LPF (") + slopeNames[i] + u8(" dB/oct)\nSetează abruptimea filtrului trece-jos din detectorul sidechain."));
        } else {
            hpfSlopeBtns[static_cast<size_t>(i)].setTooltip(juce::String("HPF Slope (") + slopeNames[i] + " dB/oct)\nSets Butterworth High-Pass roll-off steepness in sidechain detector.");
            lpfSlopeBtns[static_cast<size_t>(i)].setTooltip(juce::String("LPF Slope (") + slopeNames[i] + " dB/oct)\nSets Butterworth Low-Pass roll-off steepness in sidechain detector.");
        }
    }

    for (size_t i = 0; i < 3; ++i)
        sugPresetBtns[i].setTooltip(tr(
            "Suggested Preset\nApplies this signal-matched factory preset.",
            "Preset Sugerat\nAplică acest preset recomandat automat pe baza analizei semnalului."
        ));
    sugCloseBtn.setTooltip(tr(
        "Close Suggestions\nDismisses the signal analyzer suggestion banner.",
        "Închide Sugestiile\nAscunde panoul cu preseturile sugerate de analizator."
    ));

    autoRelBtn.setTooltip(tr(
        "Auto Release\nEnables program-dependent dual-stage release adapting to transients vs sustained notes.",
        "Auto Release\nActivează revenirea adaptivă în două etape, rapidă pe tranziții și lentă pe sunete susținute."
    ));
    parallelBtn.setTooltip(tr(
        "Overall Parallel Compression\nKeeps Dry signal intact (via Dry Gain) and sums compressed Wet signal additively.",
        "Compresie Paralelă Globală\nPăstrează semnalul Dry intact (controlat de Dry Gain) și adaugă aditiv semnalul comprimat Wet."
    ));
    autoGainBtn.setTooltip(tr(
        "Auto Makeup Gain\nAutomatically estimates makeup gain based on Threshold, Ratio, Knee, and Range.",
        "Auto Makeup Gain\nCompensează automat nivelul pierdut prin compresie în funcție de Threshold, Ratio, Knee și Range."
    ));
    lookaheadBtn.setTooltip(tr(
        "Lookahead Switch\nEnables sample-accurate lookahead delay (with DAW PDC) to catch peaks before overshoot.",
        "Comutator Lookahead\nActivează întârzierea anticipativă (compensată automat în DAW) pentru prinderea vârfurilor înainte de prag."
    ));
    scIntBtn.setTooltip(tr(
        "Internal Sidechain\nTriggers compression from the main audio input.",
        "Sidechain Intern\nDeclanșează compresia direct din semnalul audio de intrare al pistei."
    ));
    scExtBtn.setTooltip(tr(
        "External Sidechain\nTriggers compression from the external sidechain input bus.",
        "Sidechain Extern\nDeclanșează compresia dintr-o sursă externă rutată pe intrarea de sidechain din DAW."
    ));
    scAudBtn.setTooltip(tr(
        "Sidechain Audition\nRoutes the filtered sidechain detector signal directly to output for monitoring.",
        "Audiție Sidechain\nTrimite semnalul filtrat din detector direct la ieșire pentru a auzi exact ce declanșează compresia."
    ));
    scHpfBtn.setTooltip(tr(
        "Sidechain HPF\nAttenuates sub-bass frequencies in the detector so kick doesn't pump the mix.",
        "Sidechain HPF (Trece-Sus)\nActivează filtrul trece-sus în detector pentru ca sub-bass-ul să nu declanșeze pompări nedorite."
    ));
    scLpfBtn.setTooltip(tr(
        "Sidechain LPF\nAttenuates high frequencies in the detector to ignore bright cymbals or sibilance.",
        "Sidechain LPF (Trece-Jos)\nActivează filtrul trece-jos în detector pentru a ignora înaltele agresive sau cinelele."
    ));
    scEqBtn.setTooltip(tr(
        "Sidechain Bell EQ\nEnables a parametric Bell filter in the detector to emphasize or duck specific frequencies.",
        "Sidechain Bell EQ\nActivează filtrul parametric tip clopot în detector pentru a accentua sau atenua o zonă de frecvențe."
    ));

    threshSlider.setTooltip(tr(
        "Threshold (-60 to 0 dB)\nSets the level where gain reduction starts.",
        "Threshold (-60 până la 0 dB)\nStabilește nivelul de la care începe compresia; semnalul peste prag este atenuat."
    ));
    ratioSlider.setTooltip(tr(
        "Ratio (1:1 to inf:1)\nControls compression strength above Threshold.",
        "Ratio (1:1 până la inf:1)\nControlează intensitatea compresiei deasupra pragului, de la lipire ușoară până la limitare."
    ));
    kneeSlider.setTooltip(tr(
        "Knee (0 to 72 dB)\nSoftens the compression transition around Threshold.",
        "Knee (0 până la 72 dB)\nRotunjește tranziția compresiei în jurul pragului, de la hard knee (0 dB) la soft knee progresiv."
    ));
    rangeSlider.setTooltip(tr(
        "Range (0 to 60 dB)\nCaps maximum gain reduction depth to preserve peak contrast.",
        "Range (0 până la 60 dB)\nLimitează reducerea maximă de câștig permisă pentru a păstra contrastul dinamic natural."
    ));
    attackSlider.setTooltip(tr(
        "Attack (0.005 to 250 ms)\nControls how fast compression reacts to rising transients.",
        "Attack (0.005 până la 250 ms)\nDetermină cât de rapid reacționează compresorul la apariția tranzițiilor peste prag."
    ));
    holdSlider.setTooltip(tr(
        "Hold (0 to 500 ms)\nHolds peak gain reduction briefly before Release starts.",
        "Hold (0 până la 500 ms)\nMenține compresia la nivelul maxim pentru scurt timp înainte ca faza de Release să înceapă."
    ));
    releaseSlider.setTooltip(tr(
        "Release (10 to 2500 ms)\nControls recovery speed back to 0 dB gain reduction.",
        "Release (10 până la 2500 ms)\nControlează viteza cu care compresorul revine la 0 dB reducere după scăderea semnalului."
    ));
    charSlider.setTooltip(tr(
        "Coloriture Drive (0 to 100%)\nControls analog harmonic saturation intensity.",
        "Coloriture Drive (0–100%)\nReglează cantitatea de saturație armonică analogică aplicată în modul Coloriture selectat."
    ));
    mixSlider.setTooltip(tr(
        "Mix (0 to 100%)\nBlends Dry/Wet or scales additive Wet signal in Parallel mode.",
        "Mix (0–100%)\nCombină semnalul Dry cu Wet în modul standard, sau reglează nivelul căii Wet în modul Parallel."
    ));
    gainSlider.setTooltip(tr(
        "Wet Gain (-18 to +36 dB)\nPost-compression makeup gain for the compressed Wet path.",
        "Wet Gain (-18 până la +36 dB)\nAmplificare de compensare aplicată exclusiv semnalului comprimat (Wet)."
    ));
    dryGainSlider.setTooltip(tr(
        "Dry Gain (-inf to +12 dB)\nIndependent level trim for the uncompressed Dry path.",
        "Dry Gain (-inf până la +12 dB)\nNivel independent pentru semnalul necomprimat (Dry), esențial în compresia paralelă."
    ));
    lookaheadSlider.setTooltip(tr(
        "Lookahead Time (0 to 20 ms)\nAnticipation window before transients reach the compressor.",
        "Timp Lookahead (0–20 ms)\nFereastra de anticipare în milisecunde înainte ca semnalul să atingă detectorul."
    ));
    linkSlider.setTooltip(tr(
        "Stereo Link (0 to 100%)\nBlends between dual-mono L/R detection and linked stereo detection.",
        "Stereo Link (0–100%)\nCombină detecția independentă dual-mono (0%) cu detecția comună stereo (100%)."
    ));
    msSlider.setTooltip(tr(
        "M/S Balance\nBiases detection and compression toward Mid or Side in Stereo mode.",
        "Balans M/S (Mid/Side)\nÎnclină detecția și compresia spre centrul Mid sau spre lateralele Side în modul Stereo."
    ));
    scHpfSlider.setTooltip(tr(
        "Sidechain HPF Frequency\nCutoff frequency below which detector energy is rolled off.",
        "Frecvență HPF Sidechain\nFrecvența de tăiere sub care energia joasă este atenuată din circuitul de detecție."
    ));
    scLpfSlider.setTooltip(tr(
        "Sidechain LPF Frequency\nCutoff frequency above which detector energy is rolled off.",
        "Frecvență LPF Sidechain\nFrecvența de tăiere peste care frecvențele înalte sunt atenuate din detector."
    ));
    scEqFreqSlider.setTooltip(tr(
        "SC Bell Frequency\nCenter frequency of the parametric sidechain Bell EQ.",
        "Frecvență Bell Sidechain\nFrecvența centrală a filtrului parametric tip clopot din detectorul sidechain."
    ));
    scEqGainSlider.setTooltip(tr(
        "SC Bell Gain\nBoosts or cuts target detector frequency sensitivity.",
        "Câștig Bell Sidechain\nAmplifică sau atenuează sensibilitatea compresorului în banda de frecvență selectată."
    ));
    scEqQSlider.setTooltip(tr(
        "SC Bell Q\nControls bandwidth sharpness (0.3 wide to 8.0 narrow) of the Bell EQ.",
        "Factor Q Bell Sidechain\nControlează lățimea de bandă a filtrului (0.3 larg până la 8.0 îngust și selectiv)."
    ));
}

juce::String FidiuCompressorEditor::getTooltip() {
    const bool isRo = audioProcessor.uiLanguage.load(std::memory_order_relaxed) == 1;
    const auto pos = getMouseXYRelative();
    if (metersBounds.contains(pos)) {
        return juce::String::fromUTF8(isRo
            ? "Nivelmetre IN / GR / OUT\nAfișează nivelul de intrare (IN), reducerea de câștig în timp real (GR) și nivelul de ieșire (OUT) în dBFS. Click pentru resetare peak-hold."
            : "IN / GR / OUT Meters\nDisplays Input peak-hold, real-time Gain Reduction (top-down), and Output peak-hold levels in dBFS. Click meters to reset peak holds.");
    }
    if (displayBounds.contains(pos)) {
        if (displayMode == DisplayMode::Dynamics) {
            return juce::String::fromUTF8(isRo
                ? "Afișaj Dinamică\nAfișează anvelopele Input/Output și curba de reducere a câștigului (GR). Trage linia cyan vertical pentru a regla Threshold."
                : "Dynamics Display\nShows scrolling Input/Output envelopes and Gain Reduction trace. Drag the cyan dashed line vertically to set Threshold.");
        }
        return juce::String::fromUTF8(isRo
            ? "Sidechain EQ & FFT 4096-pt\nAfișează spectrul sidechain și curba interactivă a filtrelor. Trage punctele pentru acordaj; rotița mouse-ului reglează Q sau panta."
            : "Sidechain EQ & 4096-pt FFT\nDisplays sidechain spectrum and interactive filter curve. Drag nodes to tune; scroll mouse wheel to adjust Q or slope.");
    }
    return {};
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

void FidiuCompressorEditor::stepPreset(int dir) {
    const int count = presetBox.getNumItems();
    if (count <= 0) return;
    const int curIdx = std::max(0, presetBox.getSelectedId() - 1);
    const int nextIdx = (curIdx + dir + count) % count;
    presetBox.setSelectedId(nextIdx + 1, juce::dontSendNotification);
    applyPreset(nextIdx);
}

void FidiuCompressorEditor::applyPreset(int idx) {
    struct PresetDef {
        int style, colorMode, os, chMode;
        float thresh, ratio, knee, range, att, hold, rel;
        bool autoRel, parallelOn, autoGain, laOn, scExt;
        float charPct, mix, wetGain, dryGain, laMs, link;
        bool hpfOn, lpfOn, eqOn;
        float hpfFreq, lpfFreq, eqFreq, eqGain, eqQ;
        int hpfSlope, lpfSlope;
    };

    static constexpr PresetDef presets[20] = {
        { 5, 1, 1, 0, -18.0f,  4.0f, 12.0f, 36.0f, 12.0f,  8.0f, 140.0f, true,  false, false, true,  false, 24.0f, 100.0f, 2.4f, 0.0f,  4.0f, 100.0f, true,  false, false,   95.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 2, 1 },
        { 4, 3, 2, 0, -14.5f,  2.2f, 24.0f, 12.0f, 28.0f,  0.0f, 220.0f, true,  false, true,  true,  false, 14.0f, 100.0f, 1.5f, 0.0f,  2.5f,  90.0f, true,  false, false,   65.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 3, 2, 1, 0, -21.0f,  3.5f, 16.0f, 24.0f,  6.5f, 12.0f, 160.0f, true,  false, false, true,  false, 32.0f, 100.0f, 4.0f, 0.0f,  3.0f, 100.0f, true,  false, true,   130.0f, 12000.0f, 3200.0f, 3.5f, 1.4f, 2, 1 },
        { 6, 1, 2, 0, -26.0f, 10.0f,  6.0f, 42.0f, 18.0f, 15.0f,  85.0f, false, true,  false, false, false, 58.0f,  52.0f, 6.5f, 0.0f,  0.0f, 100.0f, true,  true,  false,  110.0f,  8500.0f, 2400.0f, 0.0f, 1.2f, 2, 1 },
        { 0, 0, 2, 0, -11.0f, 100.0f, 1.5f, 24.0f,  0.1f, 10.0f,  75.0f, false, false, false, true,  false,  8.0f, 100.0f, 3.0f, 0.0f, 10.0f, 100.0f, false, false, false,   30.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 7, 2, 1, 0, -24.0f,  8.0f,  8.0f, 30.0f,  1.5f, 25.0f, 175.0f, false, false, false, true,  true,  28.0f, 100.0f, 2.0f, 0.0f,  2.0f, 100.0f, true,  true,  false,   45.0f,   450.0f, 2400.0f, 0.0f, 1.2f, 1, 2 },
        { 5, 2, 1, 0, -16.5f,  2.5f, 18.0f, 18.0f, 18.0f,  4.0f, 180.0f, true,  false, false, true,  false, 34.0f, 100.0f, 1.8f, 0.0f,  2.0f, 100.0f, true,  false, false,   80.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 5, 3, 1, 2, -22.0f,  3.0f, 14.0f, 16.0f, 20.0f,  6.0f, 190.0f, true,  false, false, true,  false, 18.0f, 100.0f, 1.2f, 0.0f,  2.5f, 100.0f, true,  false, false,  140.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 2, 1 },
        { 4, 0, 2, 0, -12.5f,  2.0f, 20.0f,  8.0f, 35.0f,  0.0f, 210.0f, true,  false, true,  true,  false, 10.0f, 100.0f, 1.4f, 0.0f,  3.5f,  92.0f, true,  false, false,   55.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 4, 1, 2, 1, -15.5f,  1.8f, 16.0f, 10.0f, 25.0f,  2.0f, 185.0f, true,  false, true,  true,  false, 22.0f, 100.0f, 1.2f, 0.0f,  2.0f, 100.0f, true,  false, false,   70.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 6, 1, 1, 0, -20.0f,  5.5f,  6.0f, 28.0f, 22.0f, 12.0f,  70.0f, false, false, false, false, false, 36.0f, 100.0f, 3.2f, 0.0f,  0.0f, 100.0f, true,  true,  false,   45.0f,  3500.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 1, 3, 1, 0, -19.0f,  6.5f,  4.0f, 32.0f, 15.0f,  8.0f,  85.0f, false, false, false, true,  false, 30.0f, 100.0f, 3.5f, 0.0f,  1.0f, 100.0f, true,  false, true,    35.0f, 12000.0f,   65.0f, 4.0f, 1.5f, 1, 1 },
        { 2, 1, 1, 0, -22.0f,  4.5f, 10.0f, 28.0f,  8.0f, 15.0f, 110.0f, false, false, false, true,  false, 34.0f, 100.0f, 3.8f, 0.0f,  2.0f, 100.0f, true,  true,  false,   32.0f,  1200.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 6, 2, 2, 0, -25.0f,  8.0f,  8.0f, 36.0f, 12.0f, 10.0f,  90.0f, false, true,  false, false, false, 46.0f,  60.0f, 4.5f, 0.0f,  0.0f, 100.0f, true,  false, true,    75.0f, 12000.0f,  900.0f, 3.0f, 1.3f, 1, 1 },
        { 0, 2, 2, 0, -18.5f,  3.2f, 14.0f, 14.0f,  0.8f,  4.0f,  60.0f, false, false, false, true,  false, 16.0f, 100.0f, 1.5f, 0.0f,  1.5f, 100.0f, true,  false, true,  2800.0f, 12000.0f, 7500.0f, 4.5f, 1.8f, 2, 1 },
        { 2, 1, 1, 0, -19.5f,  3.5f, 14.0f, 20.0f, 14.0f,  6.0f, 150.0f, true,  false, false, true,  false, 26.0f, 100.0f, 2.5f, 0.0f,  2.0f, 100.0f, true,  false, false,  120.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 2, 1 },
        { 1, 3, 1, 0, -17.0f,  4.8f,  8.0f, 22.0f,  9.0f,  5.0f,  95.0f, false, false, false, true,  false, 30.0f, 100.0f, 2.0f, 0.0f,  1.5f, 100.0f, true,  true,  true,   140.0f,  5500.0f,  220.0f, 3.0f, 1.6f, 2, 1 },
        { 0, 0, 2, 0, -20.0f,  2.6f, 18.0f, 16.0f, 24.0f,  4.0f, 240.0f, true,  false, false, true,  false, 10.0f, 100.0f, 2.0f, 0.0f,  2.0f,  85.0f, true,  false, false,   75.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 2, 2, 1, 0, -21.0f,  3.0f, 20.0f, 18.0f, 16.0f,  8.0f, 280.0f, true,  false, false, true,  false, 30.0f, 100.0f, 2.2f, 0.0f,  3.0f,  75.0f, true,  false, false,  110.0f, 12000.0f, 2400.0f, 0.0f, 1.2f, 1, 1 },
        { 6, 3, 1, 0, -23.0f,  6.0f,  6.0f, 30.0f, 16.0f, 10.0f,  80.0f, false, true,  false, false, false, 36.0f,  65.0f, 4.0f, 0.0f,  0.0f, 100.0f, true,  false, true,   130.0f, 12000.0f, 2200.0f, 2.5f, 1.4f, 2, 1 }
    };

    const auto& pr = presets[std::clamp(idx, 0, 19)];
    auto setVal = [this](const juce::String& id, float v) {
        if (auto* param = audioProcessor.apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(v));
    };

    setChoiceParam("style", pr.style);
    setChoiceParam("colorMode", pr.colorMode);
    setChoiceParam("oversampling", pr.os);
    setChoiceParam("channelMode", pr.chMode);
    setVal("threshold", pr.thresh);
    setVal("ratio", pr.ratio);
    setVal("knee", pr.knee);
    setVal("range", pr.range);
    setVal("attack", pr.att);
    setVal("hold", pr.hold);
    setVal("release", pr.rel);
    setVal("autoRelease", pr.autoRel ? 1.0f : 0.0f);
    setVal("character", pr.charPct);
    setVal("mix", pr.mix);
    setVal("parallelMode", pr.parallelOn ? 1.0f : 0.0f);
    setVal("wetGain", pr.wetGain);
    setVal("autoGain", pr.autoGain ? 1.0f : 0.0f);
    setVal("dryGain", pr.dryGain);
    setVal("lookaheadOn", pr.laOn ? 1.0f : 0.0f);
    setVal("lookaheadMs", pr.laMs);
    setVal("stereoLink", pr.link);
    setVal("msBalance", 0.0f);
    setVal("scExt", pr.scExt ? 1.0f : 0.0f);
    setVal("scHpfOn", pr.hpfOn ? 1.0f : 0.0f);
    setVal("scHpfFreq", pr.hpfFreq);
    setChoiceParam("scHpfSlope", pr.hpfSlope);
    setVal("scLpfOn", pr.lpfOn ? 1.0f : 0.0f);
    setVal("scLpfFreq", pr.lpfFreq);
    setChoiceParam("scLpfSlope", pr.lpfSlope);
    setVal("scEqOn", pr.eqOn ? 1.0f : 0.0f);
    setVal("scEqFreq", pr.eqFreq);
    setVal("scEqGain", pr.eqGain);
    setVal("scEqQ", pr.eqQ);
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

    const int lang = std::clamp(audioProcessor.uiLanguage.load(std::memory_order_relaxed), 0, 1);
    if (lang != lastAppliedLang)
        applyTooltipLanguage();
}

void FidiuCompressorEditor::startSignalAnalysis() {
    if (isAnalyzing) return;
    isAnalyzing = true;
    analyzeFramesLeft = 36;
    analyzeActiveFrames = 0;
    bandAccum.fill(0.0);
    centroidNumer = 0.0;
    centroidDenom = 0.0;
    crestAccum = 0.0;
    sideAccum = 0.0;
    analyzeBtn.setButtonText("Analyzing...");
    analyzeBtn.setToggleState(true, juce::dontSendNotification);
}

void FidiuCompressorEditor::sampleAnalysisFrame() {
    const int wIdx = audioProcessor.inRingIdx.load(std::memory_order_relaxed);
    for (int i = 0; i < 4096; ++i) {
        const int rIdx = (wIdx + i) % 4096;
        inFftWork[static_cast<size_t>(i)] = audioProcessor.inFftRing[static_cast<size_t>(rIdx)].load(std::memory_order_relaxed);
    }
    std::fill(inFftWork.begin() + 4096, inFftWork.end(), 0.0f);
    scWindow.multiplyWithWindowingTable(inFftWork.data(), 4096);
    scFft.performFrequencyOnlyForwardTransform(inFftWork.data());

    const float rmsDb = audioProcessor.currentInRmsDb.load(std::memory_order_relaxed);
    const float crestDb = audioProcessor.currentInCrestDb.load(std::memory_order_relaxed);
    const float sideRatio = audioProcessor.currentInSideRatio.load(std::memory_order_relaxed);
    const double sr = audioProcessor.currentSampleRate.load(std::memory_order_relaxed);
    const double binHz = (sr * 0.5) / 2048.0;

    if (rmsDb > -60.0f) {
        ++analyzeActiveFrames;
        crestAccum += static_cast<double>(crestDb);
        sideAccum += static_cast<double>(sideRatio);
        for (int b = 1; b < 2048; ++b) {
            const double freq = static_cast<double>(b) * binHz;
            if (freq < 20.0 || freq > 18000.0) continue;
            const double mag = static_cast<double>(inFftWork[static_cast<size_t>(b)]) / 1024.0;
            const double pwr = mag * mag;
            centroidDenom += pwr;
            centroidNumer += pwr * freq;
            if (freq < 85.0) bandAccum[0] += pwr;
            else if (freq < 220.0) bandAccum[1] += pwr;
            else if (freq < 800.0) bandAccum[2] += pwr;
            else if (freq < 4500.0) bandAccum[3] += pwr;
            else bandAccum[4] += pwr;
        }
    }

    --analyzeFramesLeft;
    if (analyzeFramesLeft <= 0)
        finalizeSignalAnalysis();
}

void FidiuCompressorEditor::finalizeSignalAnalysis() {
    isAnalyzing = false;
    analyzeBtn.setButtonText("Analyze Signal");
    analyzeBtn.setToggleState(false, juce::dontSendNotification);

    if (analyzeActiveFrames == 0 || centroidDenom < 1e-12) {
        analyzedTitle = "No Input Signal";
        analyzedSub = "Play audio through Fidiu Compressor and click Analyze Signal.";
        sugPresetIndices = { 0, 1, 3 };
    } else {
        const double invE = 1.0 / centroidDenom;
        const double subPct = bandAccum[0] * invE;
        const double bassPct = bandAccum[1] * invE;
        const double lowMidPct = bandAccum[2] * invE;
        const double highMidPct = bandAccum[3] * invE;
        const double highPct = bandAccum[4] * invE;
        const double lowPct = subPct + bassPct;
        const double midPct = lowMidPct + highMidPct;
        const double avgCrest = crestAccum / static_cast<double>(analyzeActiveFrames);
        const double avgSide = sideAccum / static_cast<double>(analyzeActiveFrames);
        const double centroidHz = centroidNumer * invE;

        if (lowPct > 0.60 && subPct > 0.22 && avgCrest >= 11.2 && avgSide < 0.30) {
            analyzedTitle = "Kick / Sub Punch";
            analyzedSub = "Sub-heavy transient punch | Crest " + juce::String(avgCrest, 1) + " dB";
            sugPresetIndices = { 10, 11, 3 };
        } else if (lowPct > 0.54 && avgCrest < 11.2 && highPct < 0.10 && avgSide < 0.32) {
            analyzedTitle = "Bass / Sub Bass";
            analyzedSub = "Sustained low-end | Crest " + juce::String(avgCrest, 1) + " dB";
            sugPresetIndices = { 12, 13, 0 };
        } else if (highPct > 0.36 || (centroidHz > 3600.0 && lowPct < 0.16)) {
            analyzedTitle = "Hats / High Percussion";
            analyzedSub = "Bright high transients | " + juce::String(centroidHz / 1000.0, 1) + " kHz centroid";
            sugPresetIndices = { 14, 4, 3 };
        } else if (avgSide > 0.46 && avgCrest < 11.0 && midPct > 0.35) {
            analyzedTitle = "Synths / Wide Stereo Pad";
            analyzedSub = "Wide stereo field (" + juce::String(static_cast<int>(std::round(avgSide * 100.0))) + "% Side)";
            sugPresetIndices = { 18, 7, 19 };
        } else if (highMidPct > 0.28 && midPct > 0.56 && avgSide < 0.36 && subPct < 0.14) {
            if (avgCrest >= 12.2 && highPct > 0.10) {
                analyzedTitle = "Piano / Acoustic Keys";
                analyzedSub = "Dynamic midrange expression | Crest " + juce::String(avgCrest, 1) + " dB";
                sugPresetIndices = { 17, 15, 1 };
            } else if (avgCrest < 10.6 && lowMidPct > 0.24) {
                analyzedTitle = "Electric Guitar / Crunch";
                analyzedSub = "Dense midrange harmonics | " + juce::String(static_cast<int>(std::round(centroidHz))) + " Hz";
                sugPresetIndices = { 16, 15, 6 };
            } else {
                analyzedTitle = "Vocal / Lead Presence";
                analyzedSub = "Presence-centered mid | Crest " + juce::String(avgCrest, 1) + " dB";
                sugPresetIndices = { 2, 9, 15 };
            }
        } else if (highMidPct > 0.25 && avgCrest >= 11.2 && subPct < 0.22) {
            analyzedTitle = avgSide >= 0.26 ? juce::String("Synths / Transient Pluck") : juce::String("Acoustic Guitar / Pluck");
            analyzedSub = "Snappy mid-high transients | Crest " + juce::String(avgCrest, 1) + " dB";
            sugPresetIndices = avgSide >= 0.26 ? std::array<int, 3>{ 19, 15, 18 } : std::array<int, 3>{ 15, 19, 17 };
        } else if (avgCrest >= 11.8 && lowPct > 0.22 && (highMidPct + highPct) > 0.18) {
            analyzedTitle = "Drum Bus / Rhythm Mix";
            analyzedSub = "Wideband transients + sub weight | Crest " + juce::String(avgCrest, 1) + " dB";
            sugPresetIndices = { 3, 0, 6 };
        } else if (avgCrest < 9.2 && lowPct > 0.16 && (highMidPct + highPct) > 0.16) {
            analyzedTitle = "Mastering / Full-Range";
            analyzedSub = "Dense balanced spectrum | Crest " + juce::String(avgCrest, 1) + " dB";
            sugPresetIndices = { 1, 8, 9 };
        } else {
            analyzedTitle = "Full Mix Bus";
            analyzedSub = "Balanced spectrum | Crest " + juce::String(avgCrest, 1) + " dB";
            sugPresetIndices = avgSide > 0.36 ? std::array<int, 3>{ 0, 7, 6 } : std::array<int, 3>{ 0, 6, 1 };
        }
    }

    for (size_t i = 0; i < 3; ++i) {
        const int itemId = std::clamp(sugPresetIndices[i], 0, 19) + 1;
        sugPresetBtns[i].setButtonText(presetBox.getItemText(presetBox.indexOfItemId(itemId)));
        sugPresetBtns[i].setVisible(true);
    }
    sugCloseBtn.setVisible(true);
    hasSuggestion = true;
    repaint();
}

void FidiuCompressorEditor::timerCallback() {
    updateButtonStates();
    if (copyFlashFrames > 0) {
        --copyFlashFrames;
        if (copyFlashFrames == 0)
            copyAbBtn.setButtonText("Copy");
    }
    if (isAnalyzing) {
        sampleAnalysisFrame();
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
        g.drawText(label, static_cast<int>(nx - 64), static_cast<int>(ny - 24), 128, 15, juce::Justification::centred, false);
    };
    const juce::String hpfTxt = "HPF " + (hpfF >= 1000.0f ? juce::String(hpfF / 1000.0f, 1) + "k" : juce::String(static_cast<int>(std::round(hpfF))) + "Hz") + " (" + juce::String(hpfS) + "dB)";
    const juce::String lpfTxt = "LPF " + (lpfF >= 1000.0f ? juce::String(lpfF / 1000.0f, 1) + "k" : juce::String(static_cast<int>(std::round(lpfF))) + "Hz") + " (" + juce::String(lpfS) + "dB)";
    const juce::String eqTxt  = "EQ " + (eqG >= 0.0f ? juce::String("+") : juce::String("")) + juce::String(eqG, 1) + "dB Q" + juce::String(eqQ, 1);
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

    // Top Toolbar vertical group separators
    for (int dx : topDividers) {
        g.setColour(juce::Colour(0xff3a4254));
        g.drawVerticalLine(dx, 7.0f, 35.0f);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawVerticalLine(dx + 1, 7.0f, 35.0f);
    }

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

    // Signal Analysis Preset Suggestion Overlay Card
    if (hasSuggestion) {
        auto sRect = suggestionBounds.toFloat();
        g.setColour(juce::Colour(0xeb141821));
        g.fillRoundedRectangle(sRect, 5.0f);
        g.setColour(kCyan.withAlpha(0.45f));
        g.drawRoundedRectangle(sRect, 5.0f, 1.0f);

        g.setColour(kCyan);
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(analyzedTitle, suggestionBounds.getX() + 10, suggestionBounds.getY() + 5, 190, 16, juce::Justification::left, false);
        g.setColour(kMutedText);
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(analyzedSub, suggestionBounds.getX() + 196, suggestionBounds.getY() + 5, suggestionBounds.getWidth() - 230, 16, juce::Justification::left, false);
    }

    paintMeters(g, metersBounds.toFloat());

    // Console Gradient
    juce::ColourGradient conGrad(kBgTopBar, 0.0f, static_cast<float>(consoleBounds.getY()),
                                 kBgConsole, 0.0f, static_cast<float>(consoleBounds.getBottom()), false);
    g.setGradientFill(conGrad);
    g.fillRect(consoleBounds);
    g.setColour(kBorder);
    g.drawHorizontalLine(consoleBounds.getY(), 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(consoleBounds.getY() + 38, 14.0f, static_cast<float>(getWidth() - 14));

    // Style / Coloriture divider & labels
    g.setColour(kBorder);
    g.drawVerticalLine(getWidth() - 330, static_cast<float>(consoleBounds.getY() + 6), static_cast<float>(consoleBounds.getY() + 32));
    g.setColour(juce::Colour(0xff7d8799));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("STYLE", 16, consoleBounds.getY() + 10, 44, 20, juce::Justification::left);
    g.drawText("COLORITURE", getWidth() - 322, consoleBounds.getY() + 10, 76, 20, juce::Justification::right);

    // Knob console group dividers
    for (int cx : conDividers) {
        g.setColour(juce::Colour(0xff363d4d));
        g.drawVerticalLine(cx, static_cast<float>(consoleBounds.getY() + 54), static_cast<float>(consoleBounds.getBottom() - 12));
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine(cx + 1, static_cast<float>(consoleBounds.getY() + 54), static_cast<float>(consoleBounds.getBottom() - 12));
    }

    // Lower Lookahead & Sidechain Rail
    g.setColour(kBgRail);
    g.fillRect(scRailBounds);
    g.setColour(kBorder);
    g.drawHorizontalLine(scRailBounds.getY(), 0.0f, static_cast<float>(getWidth()));

    // Clean vertical dividers between Lower Sidechain Rail button/knob groups
    for (int rx : railDividers) {
        g.setColour(juce::Colour(0xff363d4d));
        g.drawVerticalLine(rx, static_cast<float>(scRailBounds.getY() + 18), static_cast<float>(scRailBounds.getBottom() - 18));
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine(rx + 1, static_cast<float>(scRailBounds.getY() + 18), static_cast<float>(scRailBounds.getBottom() - 18));
    }

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

    // Top Toolbar (< Preset > | A B Copy Reset | Analyze Signal ... Level Match | Bypass)
    topDividers[0] = 224;
    presetPrevBtn.setBounds(230, 8, 22, 26);
    presetBox.setBounds(253, 8, 158, 26);
    presetNextBtn.setBounds(412, 8, 22, 26);
    topDividers[1] = 439;
    bankABtn.setBounds(445, 8, 28, 26);
    bankBBtn.setBounds(474, 8, 28, 26);
    copyAbBtn.setBounds(505, 8, 50, 26);
    resetBtn.setBounds(558, 8, 46, 26);
    topDividers[2] = 609;
    analyzeBtn.setBounds(615, 8, 102, 26);

    topDividers[3] = w - 260;
    langBtn.setBounds(w - 252, 8, 48, 26);
    levelMatchBtn.setBounds(w - 198, 8, 94, 26);
    topDividers[4] = w - 98;
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

    // Signal Suggestion Strip (top-right inside display, below LA/GR readout)
    const int sugW = std::min(510, displayBounds.getWidth() - 60);
    suggestionBounds = { displayBounds.getRight() - sugW - 14, topH + 34, sugW, 56 };
    const int btnW = (sugW - 46) / 3;
    for (int i = 0; i < 3; ++i)
        sugPresetBtns[static_cast<size_t>(i)].setBounds(suggestionBounds.getX() + 10 + i * (btnW + 6), suggestionBounds.getY() + 26, btnW, 22);
    sugCloseBtn.setBounds(suggestionBounds.getRight() - 26, suggestionBounds.getY() + 4, 20, 18);

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

    conDividers[0] = 16 + colW * 1;
    conDividers[1] = 16 + colW * 4;
    conDividers[2] = 16 + colW * 7;

    // Integrated Lower Lookahead, Mid/Side/Stereo & Sidechain Rail
    const int ry = scRailBounds.getY() + 18;
    const int extraGap = std::clamp((w - 1010) / 4, 0, 42);
    int rx = 16;
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
    rx += 58;
    railDividers[0] = rx + 6 + extraGap / 2;
    rx += 12 + extraGap;

    // Sidechain routing
    scIntBtn.setBounds(rx, ry + 2, 47, 20);
    scExtBtn.setBounds(rx + 49, ry + 2, 47, 20);
    scAudBtn.setBounds(rx, ry + 28, 96, 20);
    rx += 98;
    railDividers[1] = rx + 6 + extraGap / 2;
    rx += 12 + extraGap;

    // HPF
    scHpfBtn.setBounds(rx, ry + 2, 95, 20);
    for (int i = 0; i < 4; ++i)
        hpfSlopeBtns[static_cast<size_t>(i)].setBounds(rx + i * 24, ry + 28, 23, 19);
    rx += 100;
    scHpfSlider.setBounds(rx, ry, 54, 74); rx += 56;
    railDividers[2] = rx + 6 + extraGap / 2;
    rx += 12 + extraGap;

    // LPF
    scLpfBtn.setBounds(rx, ry + 2, 95, 20);
    for (int i = 0; i < 4; ++i)
        lpfSlopeBtns[static_cast<size_t>(i)].setBounds(rx + i * 24, ry + 28, 23, 19);
    rx += 100;
    scLpfSlider.setBounds(rx, ry, 54, 74); rx += 56;
    railDividers[3] = rx + 6 + extraGap / 2;
    rx += 12 + extraGap;

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

void FidiuCompressorEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (displayMode != DisplayMode::Sidechain || !displayBounds.contains(e.getPosition())) {
        AudioProcessorEditor::mouseWheelMove(e, wheel);
        return;
    }

    const float dir = (std::abs(wheel.deltaY) >= std::abs(wheel.deltaX)) ? wheel.deltaY : wheel.deltaX;
    if (std::abs(dir) < 0.0001f)
        return;

    auto area = displayBounds.toFloat();
    auto plot = area.reduced(44.0f, 20.0f).withRight(area.getRight() - 16.0f).withBottom(area.getBottom() - 26.0f);
    auto eqDbY = [&](float db) { return plot.getY() + ((24.0f - std::clamp(db, -36.0f, 24.0f)) / 60.0f) * plot.getHeight(); };

    const float hpfX = freqToX(audioProcessor.apvts.getRawParameterValue("scHpfFreq")->load(), plot);
    const float lpfX = freqToX(audioProcessor.apvts.getRawParameterValue("scLpfFreq")->load(), plot);
    const float eqX = freqToX(audioProcessor.apvts.getRawParameterValue("scEqFreq")->load(), plot);
    const float eqY = eqDbY(audioProcessor.apvts.getRawParameterValue("scEqGain")->load());
    const float baseY = eqDbY(-3.0f);

    const float dEq = e.position.getDistanceFrom({ eqX, eqY });
    const float dHpf = e.position.getDistanceFrom({ hpfX, baseY });
    const float dLpf = e.position.getDistanceFrom({ lpfX, baseY });

    auto setValGesture = [this](const juce::String& id, float v) {
        if (auto* param = audioProcessor.apvts.getParameter(id)) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(v));
            param->endChangeGesture();
        }
    };

    if (dHpf < 38.0f && dHpf < dEq) {
        setValGesture("scHpfOn", 1.0f);
        const int cur = getChoiceParam("scHpfSlope");
        setChoiceParam("scHpfSlope", std::clamp(cur + (dir > 0.0f ? 1 : -1), 0, 3));
    } else if (dLpf < 38.0f && dLpf < dEq) {
        setValGesture("scLpfOn", 1.0f);
        const int cur = getChoiceParam("scLpfSlope");
        setChoiceParam("scLpfSlope", std::clamp(cur + (dir > 0.0f ? 1 : -1), 0, 3));
    } else {
        setValGesture("scEqOn", 1.0f);
        const float curQ = audioProcessor.apvts.getRawParameterValue("scEqQ")->load();
        const float factor = e.mods.isShiftDown() ? (dir > 0.0f ? 1.04f : 1.0f / 1.04f)
                                                  : (dir > 0.0f ? 1.14f : 1.0f / 1.14f);
        setValGesture("scEqQ", std::clamp(curQ * factor, 0.3f, 8.0f));
    }
}
