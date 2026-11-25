#include "PluginProcessor.h"
#include "PluginEditor.h"

FidiuCompressorProcessor::FidiuCompressorProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FidiuCompressorProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterChoice>("style", "Style",
        juce::StringArray{"Clean", "Classic", "Opto", "Vocal", "Mastering", "Bus", "Punch", "Pumping"}, 5));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("threshold", "Threshold", -60.0f, 0.0f, -18.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ratio", "Ratio",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f, 0.35f), 4.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("knee", "Knee", 0.0f, 72.0f, 12.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("range", "Range", 0.0f, 60.0f, 36.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack",
        juce::NormalisableRange<float>(0.005f, 250.0f, 0.001f, 0.3f), 12.00f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("hold", "Hold", 0.0f, 500.0f, 8.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release",
        juce::NormalisableRange<float>(10.0f, 2500.0f, 0.1f, 0.35f), 140.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("autoRelease", "Auto Release", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("character", "Coloriture", 0.0f, 100.0f, 24.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("colorMode", "Color Mode",
        juce::StringArray{"Pure", "Tube", "Tape", "Transformer"}, 1));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 100.0f, 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wetGain", "Wet Gain", -18.0f, 36.0f, 2.4f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("lookaheadOn", "Lookahead On", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("lookaheadMs", "Lookahead", 0.0f, 20.0f, 4.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("oversampling", "Oversampling",
        juce::StringArray{"Off", "2x", "4x"}, 1));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stereoLink", "Stereo Link", 0.0f, 100.0f, 100.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("scExt", "External SC", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("scAudition", "Audition SC", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("scHpfOn", "SC HPF On", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scHpfFreq", "SC HPF Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 95.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("scLpfOn", "SC LPF On", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scLpfFreq", "SC LPF Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 12000.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("scEqOn", "SC Bell On", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scEqFreq", "SC Bell Freq",
        juce::NormalisableRange<float>(40.0f, 16000.0f, 1.0f, 0.25f), 2400.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scEqGain", "SC Bell Gain", -18.0f, 18.0f, 0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scEqQ", "SC Bell Q", 0.3f, 8.0f, 1.20f));

    return { p.begin(), p.end() };
}

void FidiuCompressorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    dsp.prepare(sampleRate, samplesPerBlock);
    const bool laOn = apvts.getRawParameterValue("lookaheadOn")->load() > 0.5f;
    const float laMs = apvts.getRawParameterValue("lookaheadMs")->load();
    setLatencySamples(laOn ? static_cast<int>(std::round((laMs / 1000.0) * sampleRate)) : 0);
}

void FidiuCompressorProcessor::releaseResources() {
    dsp.reset();
}

bool FidiuCompressorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void FidiuCompressorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer(buffer, true, 0);
    auto scBus = getBusCount(true) > 1 ? getBusBuffer(buffer, true, 1) : mainBus;

    const int numSamples = mainBus.getNumSamples();
    if (numSamples == 0) return;

    const auto styleIdx = static_cast<fidiu::CompressionStyle>(static_cast<int>(apvts.getRawParameterValue("style")->load()));
    const auto style = fidiu::getStyleProfile(styleIdx);
    const auto colorIdx = static_cast<fidiu::ColorMode>(static_cast<int>(apvts.getRawParameterValue("colorMode")->load()));

    const float thresh = apvts.getRawParameterValue("threshold")->load();
    const float ratio = apvts.getRawParameterValue("ratio")->load();
    const float knee = apvts.getRawParameterValue("knee")->load() * style.kneeMult;
    const float range = apvts.getRawParameterValue("range")->load();
    const float attackMs = apvts.getRawParameterValue("attack")->load();
    const float holdMs = apvts.getRawParameterValue("hold")->load();
    const float releaseMs = apvts.getRawParameterValue("release")->load();
    const bool autoRel = apvts.getRawParameterValue("autoRelease")->load() > 0.5f;
    const float charPct = apvts.getRawParameterValue("character")->load();
    const float mix = apvts.getRawParameterValue("mix")->load() / 100.0f;
    const float wetGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("wetGain")->load());
    const bool laOn = apvts.getRawParameterValue("lookaheadOn")->load() > 0.5f;
    const float laMs = apvts.getRawParameterValue("lookaheadMs")->load();
    const float link = apvts.getRawParameterValue("stereoLink")->load() / 100.0f;
    const bool scExt = apvts.getRawParameterValue("scExt")->load() > 0.5f && scBus.getNumChannels() > 0;
    const bool scAud = apvts.getRawParameterValue("scAudition")->load() > 0.5f;
    const bool hpfOn = apvts.getRawParameterValue("scHpfOn")->load() > 0.5f;
    const float hpfFreq = apvts.getRawParameterValue("scHpfFreq")->load();
    const bool lpfOn = apvts.getRawParameterValue("scLpfOn")->load() > 0.5f;
    const float lpfFreq = apvts.getRawParameterValue("scLpfFreq")->load();
    const bool eqOn = apvts.getRawParameterValue("scEqOn")->load() > 0.5f;
    const float eqFreq = apvts.getRawParameterValue("scEqFreq")->load();
    const float eqGain = apvts.getRawParameterValue("scEqGain")->load();
    const float eqQ = apvts.getRawParameterValue("scEqQ")->load();

    dsp.updateFilters(hpfFreq, 24, lpfFreq, 12, eqFreq, eqGain, eqQ);

    const int laSamples = laOn ? std::min(static_cast<int>(dsp.delayL.size()) - 2, static_cast<int>(std::round((laMs / 1000.0) * dsp.sr))) : 0;
    setLatencySamples(laSamples);

    const float attCoeff = std::exp(-1.0f / (std::max(0.000005f, (attackMs * style.attackScale) / 1000.0f) * static_cast<float>(dsp.sr)));
    const float fastRelCoeff = std::exp(-1.0f / (std::max(0.005f, (releaseMs * style.releaseScale) / 1000.0f) * static_cast<float>(dsp.sr)));
    const float slowRelCoeff = std::exp(-1.0f / (std::max(0.18f, (releaseMs * 3.8f) / 1000.0f) * static_cast<float>(dsp.sr)));
    const float rmsCoeff = std::exp(-1.0f / (0.012f * static_cast<float>(dsp.sr)));
    const int holdSamples = static_cast<int>(std::round((holdMs / 1000.0f) * dsp.sr));
    const float laSmoothCoeff = laSamples > 2 ? std::exp(-3.2f / static_cast<float>(laSamples)) : 0.0f;

    auto* chL = mainBus.getWritePointer(0);
    auto* chR = mainBus.getNumChannels() > 1 ? mainBus.getWritePointer(1) : chL;
    const auto* scInL = scExt ? scBus.getReadPointer(0) : chL;
    const auto* scInR = scExt ? (scBus.getNumChannels() > 1 ? scBus.getReadPointer(1) : scInL) : chR;

    const float drive = std::min(1.0f, (charPct / 100.0f) * 0.85f + style.colorBase * 0.35f);
    float peakIn = 0.0f, peakOut = 0.0f, maxGr = 0.0f;
    const size_t bufCap = dsp.delayL.size();

    for (int i = 0; i < numSamples; ++i) {
        const float xL = chL[i];
        const float xR = chR[i];
        dsp.delayL[dsp.writeIdx] = xL;
        dsp.delayR[dsp.writeIdx] = xR;

        const size_t readIdx = (dsp.writeIdx + bufCap - static_cast<size_t>(laSamples)) % bufCap;
        const float dL = dsp.delayL[readIdx];
        const float dR = dsp.delayR[readIdx];

        float scL = scInL[i];
        float scR = scInR[i];
        if (hpfOn) {
            for (int s = 0; s < dsp.numHpf; ++s) {
                scL = dsp.hpfStages[static_cast<size_t>(s)].processL(scL);
                scR = dsp.hpfStages[static_cast<size_t>(s)].processR(scR);
            }
        }
        if (lpfOn) {
            for (int s = 0; s < dsp.numLpf; ++s) {
                scL = dsp.lpfStages[static_cast<size_t>(s)].processL(scL);
                scR = dsp.lpfStages[static_cast<size_t>(s)].processR(scR);
            }
        }
        if (eqOn && std::abs(eqGain) > 0.05f) {
            scL = dsp.bellStage.processL(scL);
            scR = dsp.bellStage.processR(scR);
        }

        dsp.rmsSqL = rmsCoeff * dsp.rmsSqL + (1.0f - rmsCoeff) * (scL * scL);
        dsp.rmsSqR = rmsCoeff * dsp.rmsSqR + (1.0f - rmsCoeff) * (scR * scR);
        const float detL = std::abs(scL) * (1.0f - style.rmsBlend) + std::sqrt(dsp.rmsSqL) * 1.4142f * style.rmsBlend;
        const float detR = std::abs(scR) * (1.0f - style.rmsBlend) + std::sqrt(dsp.rmsSqR) * 1.4142f * style.rmsBlend;

        if (detL >= dsp.envFastL) { dsp.envFastL = attCoeff * dsp.envFastL + (1.0f - attCoeff) * detL; dsp.holdCounterL = holdSamples; }
        else if (dsp.holdCounterL > 0) { --dsp.holdCounterL; }
        else { dsp.envFastL = fastRelCoeff * dsp.envFastL + (1.0f - fastRelCoeff) * detL; }

        if (detR >= dsp.envFastR) { dsp.envFastR = attCoeff * dsp.envFastR + (1.0f - attCoeff) * detR; dsp.holdCounterR = holdSamples; }
        else if (dsp.holdCounterR > 0) { --dsp.holdCounterR; }
        else { dsp.envFastR = fastRelCoeff * dsp.envFastR + (1.0f - fastRelCoeff) * detR; }

        const float autoAmt = autoRel ? std::max(0.35f, style.dualRelease) : style.dualRelease * 0.25f;
        dsp.envSlowL = (detL >= dsp.envSlowL) ? (attCoeff * dsp.envSlowL + (1.0f - attCoeff) * detL) : (slowRelCoeff * dsp.envSlowL + (1.0f - slowRelCoeff) * detL);
        dsp.envSlowR = (detR >= dsp.envSlowR) ? (attCoeff * dsp.envSlowR + (1.0f - attCoeff) * detR) : (slowRelCoeff * dsp.envSlowR + (1.0f - slowRelCoeff) * detR);

        const float finalEnvL = dsp.envFastL * (1.0f - autoAmt) + std::max(dsp.envFastL, dsp.envSlowL) * autoAmt;
        const float finalEnvR = dsp.envFastR * (1.0f - autoAmt) + std::max(dsp.envFastR, dsp.envSlowR) * autoAmt;
        const float linked = std::max(finalEnvL, finalEnvR);
        const float effL = finalEnvL * (1.0f - link) + linked * link;
        const float effR = finalEnvR * (1.0f - link) + linked * link;

        float grL = fidiu::FidiuDSP::computeGainReductionDb(juce::Decibels::gainToDecibels(effL, -96.0f), thresh, ratio, knee, range);
        float grR = fidiu::FidiuDSP::computeGainReductionDb(juce::Decibels::gainToDecibels(effR, -96.0f), thresh, ratio, knee, range);
        if (laSamples > 2) {
            dsp.smoothedGrL = std::max(grL, laSmoothCoeff * dsp.smoothedGrL + (1.0f - laSmoothCoeff) * grL);
            dsp.smoothedGrR = std::max(grR, laSmoothCoeff * dsp.smoothedGrR + (1.0f - laSmoothCoeff) * grR);
            grL = dsp.smoothedGrL;
            grR = dsp.smoothedGrR;
        }

        const float compL = dL * juce::Decibels::decibelsToGain(-grL);
        const float compR = dR * juce::Decibels::decibelsToGain(-grR);
        const float satL = fidiu::FidiuDSP::saturateSample(compL, drive, colorIdx);
        const float satR = fidiu::FidiuDSP::saturateSample(compR, drive, colorIdx);

        float outL = scAud ? scL : dL * (1.0f - mix) + satL * wetGain * mix;
        float outR = scAud ? scR : dR * (1.0f - mix) + satR * wetGain * mix;
        chL[i] = outL;
        if (chR != chL) chR[i] = outR;

        dsp.writeIdx = (dsp.writeIdx + 1) % bufCap;
        peakIn = std::max({ peakIn, std::abs(dL), std::abs(dR) });
        peakOut = std::max({ peakOut, std::abs(outL), std::abs(outR) });
        maxGr = std::max({ maxGr, grL, grR });
    }

    currentInDb.store(juce::Decibels::gainToDecibels(peakIn, -96.0f));
    currentOutDb.store(juce::Decibels::gainToDecibels(peakOut, -96.0f));
    currentGrDb.store(maxGr);
}

juce::AudioProcessorEditor* FidiuCompressorProcessor::createEditor() {
    return new FidiuCompressorEditor(*this);
}

void FidiuCompressorProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FidiuCompressorProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new FidiuCompressorProcessor();
}
