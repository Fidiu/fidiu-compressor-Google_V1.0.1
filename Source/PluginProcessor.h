#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FidiuDSP.h"

class FidiuCompressorProcessor : public juce::AudioProcessor {
public:
    FidiuCompressorProcessor();
    ~FidiuCompressorProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fidiu Compressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.025; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> currentGrDb { 0.0f };
    std::atomic<float> currentInDb { -96.0f };
    std::atomic<float> currentOutDb { -96.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    fidiu::FidiuDSP dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FidiuCompressorProcessor)
};
