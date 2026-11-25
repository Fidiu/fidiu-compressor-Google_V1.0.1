#include "PluginEditor.h"

FidiuCompressorEditor::FidiuCompressorEditor(FidiuCompressorProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(960, 560);
    auto initKnob = [this](juce::Slider& s, const juce::Colour& col) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        s.setColour(juce::Slider::rotarySliderFillColourId, col);
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff272c36));
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(s);
    };

    const juce::Colour orange(0xffff6b2b);
    const juce::Colour cyan(0xff22d3ee);
    const juce::Colour amber(0xfff59e0b);

    initKnob(threshSlider, cyan);
    initKnob(ratioSlider, orange);
    initKnob(kneeSlider, orange);
    initKnob(rangeSlider, orange);
    initKnob(attackSlider, cyan);
    initKnob(holdSlider, cyan);
    initKnob(releaseSlider, cyan);
    initKnob(charSlider, amber);
    initKnob(mixSlider, cyan);
    initKnob(gainSlider, cyan);
    initKnob(lookaheadSlider, cyan);
    initKnob(scHpfSlider, cyan);

    threshAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "threshold", threshSlider);
    ratioAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "ratio", ratioSlider);
    kneeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "knee", kneeSlider);
    rangeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "range", rangeSlider);
    attackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "attack", attackSlider);
    holdAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hold", holdSlider);
    releaseAtt= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "release", releaseSlider);
    charAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "character", charSlider);
    mixAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "mix", mixSlider);
    gainAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "wetGain", gainSlider);
    laAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "lookaheadMs", lookaheadSlider);
    scHpfAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "scHpfFreq", scHpfSlider);

    startTimerHz(60);
}

void FidiuCompressorEditor::timerCallback() {
    grDisplay = audioProcessor.currentGrDb.load();
    repaint();
}

void FidiuCompressorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff13161c));
    g.setColour(juce::Colour(0xff181c24));
    g.fillRect(0, 0, getWidth(), 44);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("FIDIU COMPRESSOR", 18, 8, 280, 28, juce::Justification::left);

    auto meterArea = getLocalBounds().reduced(16).removeFromTop(210);
    meterArea.setTop(54);
    g.setColour(juce::Colour(0xff0c0e13));
    g.fillRoundedRectangle(meterArea.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff252a34));
    g.drawRoundedRectangle(meterArea.toFloat(), 6.0f, 1.0f);

    const float grNorm = juce::jlimit(0.0f, 1.0f, grDisplay / 36.0f);
    g.setColour(juce::Colour(0xffff6b2b));
    g.fillRect(meterArea.getX() + 12.0f, meterArea.getY() + 10.0f, (meterArea.getWidth() - 24.0f) * grNorm, 12.0f);
    g.drawText("GR: -" + juce::String(grDisplay, 1) + " dB", meterArea.getRight() - 140, meterArea.getY() + 28, 128, 20, juce::Justification::right);
}

void FidiuCompressorEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(268);
    auto row1 = area.removeFromTop(120);
    const int w = row1.getWidth() / 6;
    threshSlider.setBounds(row1.removeFromLeft(w).reduced(4));
    ratioSlider.setBounds(row1.removeFromLeft(w).reduced(4));
    kneeSlider.setBounds(row1.removeFromLeft(w).reduced(4));
    rangeSlider.setBounds(row1.removeFromLeft(w).reduced(4));
    attackSlider.setBounds(row1.removeFromLeft(w).reduced(4));
    holdSlider.setBounds(row1.removeFromLeft(w).reduced(4));

    auto row2 = area.removeFromTop(120);
    releaseSlider.setBounds(row2.removeFromLeft(w).reduced(4));
    charSlider.setBounds(row2.removeFromLeft(w).reduced(4));
    mixSlider.setBounds(row2.removeFromLeft(w).reduced(4));
    gainSlider.setBounds(row2.removeFromLeft(w).reduced(4));
    lookaheadSlider.setBounds(row2.removeFromLeft(w).reduced(4));
    scHpfSlider.setBounds(row2.removeFromLeft(w).reduced(4));
}
