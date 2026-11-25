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

    void resetPeaks();

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int kHistSize = 300;
    static constexpr int kScFftSize = 1024;

    std::array<std::atomic<float>, kHistSize> histInDb;
    std::array<std::atomic<float>, kHistSize> histOutDb;
    std::array<std::atomic<float>, kHistSize> histGrDb;
    std::atomic<int> histWriteIdx { 0 };

    std::array<std::atomic<float>, kScFftSize> scFftRing;
    std::atomic<int> scRingIdx { 0 };

    std::atomic<float> currentInDbL { -96.0f };
    std::atomic<float> currentInDbR { -96.0f };
    std::atomic<float> currentOutDbL { -96.0f };
    std::atomic<float> currentOutDbR { -96.0f };
    std::atomic<float> currentGrDb { 0.0f };
    std::atomic<float> currentScDb { -96.0f };
    std::atomic<float> peakHoldInDb { -96.0f };
    std::atomic<float> peakHoldOutDb { -96.0f };
    std::atomic<float> peakHoldGrDb { 0.0f };
    std::atomic<int> currentLatencySamples { 0 };
    std::atomic<double> currentSampleRate { 48000.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    fidiu::FidiuDSP dsp;

    int telemetrySamples = 0;
    int peakDecayCounter = 0;
    float blockPeakInL = 0.0f, blockPeakInR = 0.0f;
    float blockPeakOutL = 0.0f, blockPeakOutR = 0.0f;
    float blockPeakSc = 0.0f, blockMaxGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FidiuCompressorProcessor)
};
