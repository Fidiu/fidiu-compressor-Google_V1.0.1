#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace fidiu {

enum class CompressionStyle {
    Clean = 0, Classic, Opto, Vocal, Mastering, Bus, Punch, Pumping
};

enum class ColorMode {
    Pure = 0, Tube, Tape, Transformer
};

struct StyleProfile {
    float rmsBlend;
    float kneeMult;
    float colorBase;
    float attackScale;
    float releaseScale;
    float dualRelease;
};

inline StyleProfile getStyleProfile(CompressionStyle s) {
    switch (s) {
        case CompressionStyle::Clean:     return { 0.20f, 1.00f, 0.04f, 1.00f, 1.00f, 0.15f };
        case CompressionStyle::Classic:   return { 0.42f, 1.15f, 0.25f, 1.10f, 1.00f, 0.38f };
        case CompressionStyle::Opto:      return { 0.72f, 1.45f, 0.34f, 1.50f, 1.35f, 0.65f };
        case CompressionStyle::Vocal:     return { 0.54f, 1.25f, 0.22f, 0.85f, 0.90f, 0.48f };
        case CompressionStyle::Mastering: return { 0.65f, 1.35f, 0.12f, 1.20f, 1.15f, 0.55f };
        case CompressionStyle::Bus:       return { 0.48f, 1.20f, 0.30f, 1.00f, 1.00f, 0.42f };
        case CompressionStyle::Punch:     return { 0.15f, 0.85f, 0.28f, 0.75f, 0.85f, 0.25f };
        case CompressionStyle::Pumping:   return { 0.28f, 0.90f, 0.38f, 0.65f, 0.72f, 0.20f };
    }
    return { 0.48f, 1.20f, 0.30f, 1.00f, 1.00f, 0.42f };
}

class BiquadFilter {
public:
    void reset() { z1L = z2L = z1R = z2R = 0.0f; }

    void setButterworthHP(float freq, double sr, float q = 0.70710678f) {
        const double f = std::clamp<double>(freq, 15.0, sr * 0.45);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double cosw0 = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = static_cast<float>(((1.0 + cosw0) * 0.5) / a0);
        b1 = static_cast<float>((-(1.0 + cosw0)) / a0);
        b2 = b0;
        a1 = static_cast<float>((-2.0 * cosw0) / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
    }

    void setButterworthLP(float freq, double sr, float q = 0.70710678f) {
        const double f = std::clamp<double>(freq, 15.0, sr * 0.45);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double cosw0 = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = static_cast<float>(((1.0 - cosw0) * 0.5) / a0);
        b1 = static_cast<float>((1.0 - cosw0) / a0);
        b2 = b0;
        a1 = static_cast<float>((-2.0 * cosw0) / a0);
        a2 = static_cast<float>((1.0 - alpha) / a0);
    }

    void setPeakingEQ(float freq, double sr, float q, float gainDb) {
        const double f = std::clamp<double>(freq, 20.0, sr * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double cosw0 = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * std::max(0.2f, q));
        const double a0 = 1.0 + alpha / A;
        b0 = static_cast<float>((1.0 + alpha * A) / a0);
        b1 = static_cast<float>((-2.0 * cosw0) / a0);
        b2 = static_cast<float>((1.0 - alpha * A) / a0);
        a1 = static_cast<float>((-2.0 * cosw0) / a0);
        a2 = static_cast<float>((1.0 - alpha / A) / a0);
    }

    inline float processL(float x) {
        const float y = b0 * x + z1L;
        z1L = b1 * x - a1 * y + z2L;
        z2L = b2 * x - a2 * y;
        return y;
    }

    inline float processR(float x) {
        const float y = b0 * x + z1R;
        z1R = b1 * x - a1 * y + z2R;
        z2R = b2 * x - a2 * y;
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1L = 0.0f, z2L = 0.0f, z1R = 0.0f, z2R = 0.0f;
};

class FidiuDSP {
public:
    void prepare(double sampleRate, int maxBlockSize) {
        sr = sampleRate;
        const int maxDelaySamples = static_cast<int>(std::ceil(0.025 * sr)) + 16;
        delayL.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
        delayR.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
        writeIdx = 0;

        oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampler2x->initProcessing(static_cast<size_t>(maxBlockSize));
        oversampler4x->initProcessing(static_cast<size_t>(maxBlockSize));

        reset();
    }

    void reset() {
        std::fill(delayL.begin(), delayL.end(), 0.0f);
        std::fill(delayR.begin(), delayR.end(), 0.0f);
        for (auto& s : hpfStages) s.reset();
        for (auto& s : lpfStages) s.reset();
        bellStage.reset();
        envFastL = envFastR = envSlowL = envSlowR = 0.0f;
        rmsSqL = rmsSqR = smoothedGrL = smoothedGrR = 0.0f;
        holdCounterL = holdCounterR = 0;
        dcPrevInL = dcPrevOutL = dcPrevInR = dcPrevOutR = 0.0f;
        if (oversampler2x) oversampler2x->reset();
        if (oversampler4x) oversampler4x->reset();
    }

    void updateFilters(float hpfFreq, int hpfSlope, float lpfFreq, int lpfSlope, float eqFreq, float eqGainDb, float eqQ) {
        static constexpr float q4[2] = { 0.54119610f, 1.30656296f };
        static constexpr float q8[4] = { 0.50979558f, 0.60134489f, 0.89997622f, 2.56291545f };

        numHpf = (hpfSlope <= 12) ? 1 : (hpfSlope == 24 ? 2 : 4);
        for (int i = 0; i < numHpf; ++i) {
            float q = (numHpf == 1) ? 0.70710678f : (numHpf == 2 ? q4[i] : q8[i]);
            hpfStages[static_cast<size_t>(i)].setButterworthHP(hpfFreq, sr, q);
        }

        numLpf = (lpfSlope <= 12) ? 1 : (lpfSlope == 24 ? 2 : 4);
        for (int i = 0; i < numLpf; ++i) {
            float q = (numLpf == 1) ? 0.70710678f : (numLpf == 2 ? q4[i] : q8[i]);
            lpfStages[static_cast<size_t>(i)].setButterworthLP(lpfFreq, sr, q);
        }

        bellStage.setPeakingEQ(eqFreq, sr, eqQ, eqGainDb);
    }

    static inline float computeGainReductionDb(float levelDb, float threshDb, float ratio, float kneeDb, float rangeDb) {
        if (ratio <= 1.001f) return 0.0f;
        const float slope = 1.0f - 1.0f / (ratio >= 99.0f ? 1000.0f : ratio);
        const float halfKnee = kneeDb * 0.5f;
        float gr = 0.0f;
        if (kneeDb > 0.05f && levelDb > threshDb - halfKnee && levelDb < threshDb + halfKnee) {
            const float diff = levelDb - (threshDb - halfKnee);
            gr = slope * (diff * diff) / (2.0f * kneeDb);
        } else if (levelDb >= threshDb + halfKnee) {
            gr = slope * (levelDb - threshDb);
        }
        return std::clamp(gr, 0.0f, rangeDb);
    }

    static inline float saturateSample(float x, float drive, ColorMode mode) {
        if (drive <= 0.001f) return x;
        switch (mode) {
            case ColorMode::Pure: {
                const float k = 1.0f + drive * 0.4f;
                return std::tanh(x * k) / std::tanh(k);
            }
            case ColorMode::Tube: {
                const float d = 1.0f + drive * 1.65f;
                const float bias = 0.08f * drive;
                return (std::tanh(x * d + bias) - std::tanh(bias)) / (d * 0.92f);
            }
            case ColorMode::Tape: {
                const float d = 1.0f + drive * 2.1f;
                const float s = std::tanh(x * d) / d;
                return s * (1.0f + 0.06f * drive) - 0.04f * drive * s * s * s;
            }
            case ColorMode::Transformer: {
                const float d = 1.0f + drive * 1.9f;
                const float t = std::tanh(x * d) / std::tanh(d);
                return 0.85f * t + 0.15f * std::tanh(x * d * 1.45f) / d;
            }
        }
        return x;
    }

    double sr = 48000.0;
    std::vector<float> delayL, delayR;
    size_t writeIdx = 0;
    std::array<BiquadFilter, 4> hpfStages, lpfStages;
    BiquadFilter bellStage;
    int numHpf = 2, numLpf = 1;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x, oversampler4x;
    float envFastL = 0, envFastR = 0, envSlowL = 0, envSlowR = 0;
    float rmsSqL = 0, rmsSqR = 0, smoothedGrL = 0, smoothedGrR = 0;
    int holdCounterL = 0, holdCounterR = 0;
    float dcPrevInL = 0, dcPrevOutL = 0, dcPrevInR = 0, dcPrevOutR = 0;
};

} // namespace fidiu
