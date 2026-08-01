#ifndef HARMONIC_TREMOLO_ENGINE_HPP
#define HARMONIC_TREMOLO_ENGINE_HPP

#include <cmath>
#include <algorithm>

namespace AudioDSP {

/**
 * @brief High-Performance Fender Tri-Verb style Harmonic Tremolo Core Engine
 * 
 * Design Features:
 *  - Crossover: Cytomic State Variable Filter (SVF) delivering LP/HP split with zero phase alignment mismatch.
 *  - Anti-Phase Dual LFO Modulation: 180-degree out-of-phase amplitude modulation for High/Low bands.
 *  - Analog Tube Saturation: Asymmetric tube warmth modeling for vintage brownface harmonic character.
 *  - Variable Crossover Frequency (150Hz - 3.5kHz) & Resonant Q control.
 *  - Multi-waveform LFO (Sine, Triangle, Tube-Sine, Square).
 *  - Stereo Phase Offset (0 to 180 degrees) for 3D spatial panning.
 *  - Embedded Realtime Safe: Zero heap allocations during audio processing (`processBlock` / `processSample`).
 */
class HarmonicTremoloEngine {
public:
    enum class LFOWaveform {
        Sine = 0,
        Triangle,
        TubeSine,
        Square
    };

    HarmonicTremoloEngine() = default;
    ~HarmonicTremoloEngine() = default;

    /**
     * @brief Initialize DSP state for sample rate
     */
    void prepare(double sampleRate) {
        mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    /**
     * @brief Clear internal states (filter memory and LFO phase)
     */
    void reset() {
        mLFOPhaseL = 0.0;
        mLFOPhaseR = 0.0;
        
        mSvfL[0] = mSvfL[1] = 0.0;
        mSvfR[0] = mSvfR[1] = 0.0;

        mSmoothedRate = mRateHz;
        mSmoothedDepth = mDepth;
        mSmoothedCrossover = mCrossoverHz;
    }

    // --- Parameter Setters ---
    void setRate(float rateHz) { mRateHz = std::clamp(rateHz, 0.1f, 20.0f); }
    void setDepth(float depth) { mDepth = std::clamp(depth, 0.0f, 1.0f); }
    void setCrossoverFrequency(float freqHz) { mCrossoverHz = std::clamp(freqHz, 150.0f, 4000.0f); }
    void setResonanceQ(float q) { mQ = std::clamp(q, 0.5f, 5.0f); }
    void setWarmth(float warmth) { mWarmth = std::clamp(warmth, 0.0f, 1.0f); }
    void setStereoPhaseOffset(float degrees) { mStereoPhaseOffsetDeg = std::clamp(degrees, 0.0f, 180.0f); }
    void setWaveform(LFOWaveform wave) { mWaveform = wave; }
    void setMix(float mix) { mMix = std::clamp(mix, 0.0f, 1.0f); }

    // --- Parameter Getters ---
    float getRate() const { return mRateHz; }
    float getDepth() const { return mDepth; }
    float getCrossoverFrequency() const { return mCrossoverHz; }
    float getResonanceQ() const { return mQ; }
    float getWarmth() const { return mWarmth; }
    float getStereoPhaseOffset() const { return mStereoPhaseOffsetDeg; }
    LFOWaveform getWaveform() const { return mWaveform; }
    float getMix() const { return mMix; }

    /**
     * @brief Process a single stereo sample frame (Embedded and AU friendly)
     * @param inL Input Left sample
     * @param inR Input Right sample
     * @param outL Output Left sample reference
     * @param outR Output Right sample reference
     */
    inline void processSample(float inL, float inR, float& outL, float& outR) {
        // Smooth parameters to prevent zipper noise
        constexpr float smoothingCoeff = 0.005f;
        mSmoothedRate += smoothingCoeff * (mRateHz - mSmoothedRate);
        mSmoothedDepth += smoothingCoeff * (mDepth - mSmoothedDepth);
        mSmoothedCrossover += smoothingCoeff * (mCrossoverHz - mSmoothedCrossover);

        // Update LFO phases
        const double phaseInc = mSmoothedRate / mSampleRate;
        mLFOPhaseL += phaseInc;
        if (mLFOPhaseL >= 1.0) mLFOPhaseL -= 1.0;

        double phaseROffset = (mStereoPhaseOffsetDeg / 360.0);
        mLFOPhaseR = mLFOPhaseL + phaseROffset;
        if (mLFOPhaseR >= 1.0) mLFOPhaseR -= 1.0;

        // Compute LFO values (-1.0 to +1.0)
        float lfoLowL = computeLFO(mLFOPhaseL);
        float lfoHighL = computeLFO(mLFOPhaseL + 0.5); // 180 deg out of phase

        float lfoLowR = computeLFO(mLFOPhaseR);
        float lfoHighR = computeLFO(mLFOPhaseR + 0.5);

        // Filter coefficients (Cytomic State Variable Filter)
        const float g = std::tan(static_cast<float>(M_PI) * mSmoothedCrossover / static_cast<float>(mSampleRate));
        const float k = 1.0f / mQ;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        // --- Process Left Channel ---
        float lowL, highL;
        stepSVF(inL, g, k, a1, a2, a3, mSvfL[0], mSvfL[1], lowL, highL);

        // --- Process Right Channel ---
        float lowR, highR;
        stepSVF(inR, g, k, a1, a2, a3, mSvfR[0], mSvfR[1], lowR, highR);

        // Apply harmonic warmth / tube distortion before modulation if enabled
        if (mWarmth > 0.001f) {
            lowL = applyTubeWarmth(lowL, mWarmth);
            highL = applyTubeWarmth(highL, mWarmth);
            lowR = applyTubeWarmth(lowR, mWarmth);
            highR = applyTubeWarmth(highR, mWarmth);
        }

        // Apply 180-degree anti-phase amplitude modulation
        float modLowGainL = 1.0f - mSmoothedDepth * 0.5f * (1.0f + lfoLowL);
        float modHighGainL = 1.0f - mSmoothedDepth * 0.5f * (1.0f + lfoHighL);

        float modLowGainR = 1.0f - mSmoothedDepth * 0.5f * (1.0f + lfoLowR);
        float modHighGainR = 1.0f - mSmoothedDepth * 0.5f * (1.0f + lfoHighR);

        float wetL = (lowL * modLowGainL) + (highL * modHighGainL);
        float wetR = (lowR * modLowGainR) + (highR * modHighGainR);

        // Dry/Wet Mix
        outL = inL * (1.0f - mMix) + wetL * mMix;
        outR = inR * (1.0f - mMix) + wetR * mMix;
    }

    /**
     * @brief Block processing function for Audio Units / DAW buffers
     */
    void processBlock(const float* const* inputChannels, float* const* outputChannels, int numChannels, int numSamples) {
        if (numChannels == 0 || numSamples == 0) return;

        const float* leftIn = inputChannels[0];
        const float* rightIn = (numChannels > 1) ? inputChannels[1] : leftIn;

        float* leftOut = outputChannels[0];
        float* rightOut = (numChannels > 1) ? outputChannels[1] : leftOut;

        for (int i = 0; i < numSamples; ++i) {
            processSample(leftIn[i], rightIn[i], leftOut[i], rightOut[i]);
        }
    }

private:
    /**
     * @brief Cytomic State Variable Filter step calculation
     */
    inline void stepSVF(float input, float g, float k, float a1, float a2, float a3, 
                        double& ic1eq, double& ic2eq, float& outLP, float& outHP) const {
        float v3 = input - static_cast<float>(ic2eq);
        float v1 = a1 * static_cast<float>(ic1eq) + a2 * v3;
        float v2 = static_cast<float>(ic2eq) + a2 * static_cast<float>(ic1eq) + a3 * v3;

        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        outLP = v2;
        outHP = input - k * v1 - v2;
    }

    /**
     * @brief Computes LFO value in range [-1.0, 1.0] for phase [0.0, 1.0]
     */
    inline float computeLFO(double phase) const {
        // Wrap phase to [0.0, 1.0)
        double p = phase - std::floor(phase);

        switch (mWaveform) {
            case LFOWaveform::Sine:
                return static_cast<float>(std::sin(2.0 * M_PI * p));

            case LFOWaveform::Triangle:
                return static_cast<float>(4.0 * std::abs(p - 0.5) - 1.0);

            case LFOWaveform::TubeSine: {
                double rawSine = std::sin(2.0 * M_PI * p);
                return static_cast<float>(std::tanh(1.8 * rawSine) / std::tanh(1.8));
            }

            case LFOWaveform::Square:
                return (p < 0.5) ? 0.95f : -0.95f;

            default:
                return static_cast<float>(std::sin(2.0 * M_PI * p));
        }
    }

    /**
     * @brief Soft-clipping asymmetric triode tube saturation model
     */
    inline float applyTubeWarmth(float sample, float warmthAmount) const {
        float drive = 1.0f + warmthAmount * 2.0f;
        float x = sample * drive;
        
        // Asymmetric triode transfer function: y = x - 0.15*x^2 - 0.05*x^3 with soft tanh cap
        float saturated = x - 0.15f * (x * x) - 0.05f * (x * x * x);
        float limit = std::tanh(saturated);
        
        // Blend between raw sample and tube saturated sample based on warmth parameter
        return sample * (1.0f - warmthAmount) + (limit / drive) * warmthAmount;
    }

    // Processing state
    double mSampleRate = 44100.0;
    
    // Parameters
    float mRateHz = 3.5f;
    float mDepth = 0.85f;
    float mCrossoverHz = 650.0f;
    float mQ = 0.707f;
    float mWarmth = 0.35f;
    float mStereoPhaseOffsetDeg = 90.0f;
    float mMix = 1.0f;
    LFOWaveform mWaveform = LFOWaveform::TubeSine;

    // Smoothed values for zipper-noise free control
    float mSmoothedRate = 3.5f;
    float mSmoothedDepth = 0.85f;
    float mSmoothedCrossover = 650.0f;

    // LFO phase counters
    double mLFOPhaseL = 0.0;
    double mLFOPhaseR = 0.0;

    // SVF filter state memory [ic1eq, ic2eq]
    double mSvfL[2] = {0.0, 0.0};
    double mSvfR[2] = {0.0, 0.0};
};

} // namespace AudioDSP

#endif // HARMONIC_TREMOLO_ENGINE_HPP
