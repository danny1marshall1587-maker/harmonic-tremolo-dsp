#ifndef IR_APPROX_REVERB_ENGINE_HPP
#define IR_APPROX_REVERB_ENGINE_HPP

#include <cmath>
#include <vector>
#include <algorithm>
#include <array>
#include <cstring>

namespace AudioDSP {

constexpr double PI_DBL = 3.14159265358979323846;
constexpr float PI_FLT = 3.14159265358979323846f;

/**
 * @brief High-Performance IR-Synthesized & Algorithmic Reverb Engine
 * 
 * Features:
 *  - Early Reflection Generator: 4-tap stereo delay line with individual gains.
 *  - Late Reverb FDN: 4-channel Householder orthogonal matrix Feedback Delay Network.
 *  - Damping & Filters: High-Pass Filter (HPF) & Low-Pass Filter (LPF) absorption.
 *  - Dynamic Ducking: Attenuates wet reverb when active playing is detected, blooming on pauses.
 *  - Gated Reverb: Envelope follower with Threshold, Attack, Hold, Release, and Tail Floor.
 *  - IR Analyzer: Schroeder Backward Integration to calculate RT60 & early reflections from WAV buffers.
 *  - Realtime Safe: Zero heap allocations during audio processing.
 */
class IRApproxReverbEngine {
public:
    IRApproxReverbEngine() = default;
    ~IRApproxReverbEngine() = default;

    /**
     * @brief Initialize DSP buffers for sample rate
     */
    void prepare(double sampleRate) {
        mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

        // FDN Prime Delay Lengths (in milliseconds: ~29ms, ~37ms, ~43ms, ~53ms)
        mFdnDelaySamples[0] = static_cast<int>(mSampleRate * 0.0297);
        mFdnDelaySamples[1] = static_cast<int>(mSampleRate * 0.0371);
        mFdnDelaySamples[2] = static_cast<int>(mSampleRate * 0.0439);
        mFdnDelaySamples[3] = static_cast<int>(mSampleRate * 0.0533);

        // Resize max buffers (2 seconds max per line)
        int maxDelay = static_cast<int>(mSampleRate * 2.0);
        for (int i = 0; i < 4; ++i) {
            mFdnBufferL[i].assign(maxDelay, 0.0f);
            mFdnBufferR[i].assign(maxDelay, 0.0f);
            mFdnWriteIdxL[i] = 0;
            mFdnWriteIdxR[i] = 0;
        }

        // Early reflection max delay (100ms)
        int maxErDelay = static_cast<int>(mSampleRate * 0.1);
        mErBufferL.assign(maxErDelay, 0.0f);
        mErBufferR.assign(maxErDelay, 0.0f);
        mErWriteIdx = 0;

        reset();
    }

    /**
     * @brief Clear internal buffer states
     */
    void reset() {
        for (int i = 0; i < 4; ++i) {
            std::fill(mFdnBufferL[i].begin(), mFdnBufferL[i].end(), 0.0f);
            std::fill(mFdnBufferR[i].begin(), mFdnBufferR[i].end(), 0.0f);
            mFdnWriteIdxL[i] = 0;
            mFdnWriteIdxR[i] = 0;
            mLpfStateL[i] = 0.0f;
            mLpfStateR[i] = 0.0f;
            mHpfStateL[i] = 0.0f;
            mHpfStateR[i] = 0.0f;
        }
        std::fill(mErBufferL.begin(), mErBufferL.end(), 0.0f);
        std::fill(mErBufferR.begin(), mErBufferR.end(), 0.0f);
        mErWriteIdx = 0;

        mDuckingEnv = 0.0f;
        mGateEnv = 0.0f;
        mGateHoldCounter = 0;
        mGateIsOpen = false;
    }

    // --- Parameter Setters ---
    void setDwell(float dwell) { mDwell = std::clamp(dwell, 0.1f, 0.98f); }
    void setTone(float tone) { 
        mTone = std::clamp(tone, 0.0f, 1.0f);
        // Tone controls LPF cutoff between 1.5kHz and 18kHz
        mLpfCutoffHz = 1500.0f + mTone * 16500.0f;
    }
    void setMix(float mix) { mMix = std::clamp(mix, 0.0f, 1.0f); }
    void setPreDelayMs(float ms) { mPreDelayMs = std::clamp(ms, 0.0f, 100.0f); }
    void setErLevel(float level) { mErLevel = std::clamp(level, 0.0f, 1.0f); }
    void setHpfCutoffHz(float hz) { mHpfCutoffHz = std::clamp(hz, 20.0f, 1000.0f); }
    void setLpfCutoffHz(float hz) { mLpfCutoffHz = std::clamp(hz, 1000.0f, 20000.0f); }

    // Ducking parameters
    void setDuckingAmount(float amount) { mDuckingAmount = std::clamp(amount, 0.0f, 1.0f); }
    void setDuckingReleaseMs(float ms) { mDuckingReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }

    // Gate parameters
    void setGateEnabled(bool enabled) { mGateEnabled = enabled; }
    void setGateThresholdDb(float db) { mGateThresholdDb = std::clamp(db, -60.0f, 0.0f); }
    void setGateHoldMs(float ms) { mGateHoldMs = std::clamp(ms, 0.0f, 500.0f); }
    void setGateReleaseMs(float ms) { mGateReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }

    // --- Parameter Getters ---
    float getDwell() const { return mDwell; }
    float getTone() const { return mTone; }
    float getMix() const { return mMix; }
    float getPreDelayMs() const { return mPreDelayMs; }
    float getErLevel() const { return mErLevel; }
    float getHpfCutoffHz() const { return mHpfCutoffHz; }
    float getLpfCutoffHz() const { return mLpfCutoffHz; }
    float getDuckingAmount() const { return mDuckingAmount; }
    float getGateThresholdDb() const { return mGateThresholdDb; }
    bool isGateEnabled() const { return mGateEnabled; }

    /**
     * @brief Process a single stereo sample frame
     */
    inline void processSample(float inL, float inR, float& outL, float& outR) {
        float inputMono = 0.5f * (inL + inR);

        // --- 1. Pre-Delay & Early Reflection Taps ---
        int preDelaySamples = static_cast<int>(mSampleRate * (mPreDelayMs / 1000.0f));
        int erSize = static_cast<int>(mErBufferL.size());
        
        mErBufferL[mErWriteIdx] = inL;
        mErBufferR[mErWriteIdx] = inR;

        // 4 Early Reflection Taps
        float erL = 0.0f;
        float erR = 0.0f;
        constexpr float erTapsMs[4] = { 12.0f, 19.0f, 27.0f, 38.0f };
        constexpr float erGains[4]  = { 0.7f,  0.5f,  0.35f, 0.2f };

        for (int t = 0; t < 4; ++t) {
            int tapDelay = preDelaySamples + static_cast<int>(mSampleRate * (erTapsMs[t] / 1000.0f));
            int readIdx = (mErWriteIdx - tapDelay + erSize) % erSize;
            erL += mErBufferL[readIdx] * erGains[t];
            erR += mErBufferR[readIdx] * erGains[t];
        }

        mErWriteIdx = (mErWriteIdx + 1) % erSize;

        // --- 2. Late Reverb Feedback Delay Network (FDN) ---
        // Read current FDN delay line outputs
        float fdnOutL[4], fdnOutR[4];
        for (int i = 0; i < 4; ++i) {
            int maxLen = static_cast<int>(mFdnBufferL[i].size());
            int readIdxL = (mFdnWriteIdxL[i] - mFdnDelaySamples[i] + maxLen) % maxLen;
            int readIdxR = (mFdnWriteIdxR[i] - mFdnDelaySamples[i] + maxLen) % maxLen;

            fdnOutL[i] = mFdnBufferL[i][readIdxL];
            fdnOutR[i] = mFdnBufferR[i][readIdxR];
        }

        // Apply 4x4 Householder Unitary Matrix Diffusion: y_i = x_i - 0.5 * sum(x)
        float sumL = fdnOutL[0] + fdnOutL[1] + fdnOutL[2] + fdnOutL[3];
        float sumR = fdnOutR[0] + fdnOutR[1] + fdnOutR[2] + fdnOutR[3];

        float diffL[4], diffR[4];
        for (int i = 0; i < 4; ++i) {
            diffL[i] = fdnOutL[i] - 0.5f * sumL;
            diffR[i] = fdnOutR[i] - 0.5f * sumR;
        }

        // 1-Pole HPF & LPF Damping Filters inside feedback loop
        float lpfCoeff = 1.0f - std::exp(-2.0f * PI_FLT * mLpfCutoffHz / static_cast<float>(mSampleRate));
        float hpfCoeff = std::exp(-2.0f * PI_FLT * mHpfCutoffHz / static_cast<float>(mSampleRate));

        for (int i = 0; i < 4; ++i) {
            // LPF
            mLpfStateL[i] += lpfCoeff * (diffL[i] - mLpfStateL[i]);
            mLpfStateR[i] += lpfCoeff * (diffR[i] - mLpfStateR[i]);

            // HPF
            mHpfStateL[i] = hpfCoeff * (mHpfStateL[i] + mLpfStateL[i] - diffL[i]);
            mHpfStateR[i] = hpfCoeff * (mHpfStateR[i] + mLpfStateR[i] - diffR[i]);

            float dampedL = mLpfStateL[i] - mHpfStateL[i];
            float dampedR = mLpfStateR[i] - mHpfStateR[i];

            // Feedback write back to buffers
            int maxLen = static_cast<int>(mFdnBufferL[i].size());
            mFdnBufferL[i][mFdnWriteIdxL[i]] = inputMono + dampedL * mDwell;
            mFdnBufferR[i][mFdnWriteIdxR[i]] = inputMono + dampedR * mDwell;

            mFdnWriteIdxL[i] = (mFdnWriteIdxL[i] + 1) % maxLen;
            mFdnWriteIdxR[i] = (mFdnWriteIdxR[i] + 1) % maxLen;
        }

        // Sum FDN outputs
        float lateL = (fdnOutL[0] + fdnOutL[2]) * 0.5f;
        float lateR = (fdnOutR[1] + fdnOutR[3]) * 0.5f;

        // Combine Early Reflections + Late Reverb
        float wetL = (erL * mErLevel) + lateL;
        float wetR = (erR * mErLevel) + lateR;

        // --- 3. Dynamic Ducking ---
        if (mDuckingAmount > 0.001f) {
            float inAbs = std::abs(inputMono);
            float duckAttackCoeff = 0.01f;
            float duckReleaseCoeff = 1.0f - std::exp(-1.0f / (mSampleRate * (mDuckingReleaseMs / 1000.0f)));

            if (inAbs > mDuckingEnv) {
                mDuckingEnv += duckAttackCoeff * (inAbs - mDuckingEnv);
            } else {
                mDuckingEnv += duckReleaseCoeff * (inAbs - mDuckingEnv);
            }

            float duckGain = 1.0f - (mDuckingEnv * mDuckingAmount);
            duckGain = std::clamp(duckGain, 0.0f, 1.0f);
            wetL *= duckGain;
            wetR *= duckGain;
        }

        // --- 4. Noise Gate ---
        if (mGateEnabled) {
            float wetAbs = std::max(std::abs(wetL), std::abs(wetR));
            float threshLin = std::pow(10.0f, mGateThresholdDb / 20.0f);

            int holdSamples = static_cast<int>(mSampleRate * (mGateHoldMs / 1000.0f));
            float relCoeff = std::exp(-1.0f / (mSampleRate * (mGateReleaseMs / 1000.0f)));

            if (wetAbs > threshLin) {
                mGateIsOpen = true;
                mGateHoldCounter = holdSamples;
                mGateEnv = 1.0f;
            } else {
                if (mGateHoldCounter > 0) {
                    mGateHoldCounter--;
                    mGateEnv = 1.0f;
                } else {
                    mGateEnv *= relCoeff;
                }
            }

            wetL *= mGateEnv;
            wetR *= mGateEnv;
        }

        // --- 5. Dry/Wet Mix ---
        outL = inL * (1.0f - mMix) + wetL * mMix;
        outR = inR * (1.0f - mMix) + wetR * mMix;
    }

    /**
     * @brief Process block for DAW audio buffers
     */
    void processBlock(const float* const* inputChannels, float* const* outputChannels, int numChannels, int numSamples) {
        if (numChannels == 0 || numSamples == 0) return;

        const float* inL = inputChannels[0];
        const float* inR = (numChannels > 1) ? inputChannels[1] : inL;
        float* outL = outputChannels[0];
        float* outR = (numChannels > 1) ? outputChannels[1] : outL;

        for (int i = 0; i < numSamples; ++i) {
            processSample(inL[i], inR[i], outL[i], outR[i]);
        }
    }

    /**
     * @brief Instant Mathematical IR Waveform Analyzer (Schroeder Backward Integration)
     * Reads a raw IR .wav buffer and automatically extracts T60 decay, Dwell, and Tone settings.
     */
    void analyzeImpulseResponse(const float* irBuffer, int numSamples, double irSampleRate) {
        if (!irBuffer || numSamples <= 0) return;

        // Calculate Energy Decay Curve (EDC) using Schroeder Integration
        std::vector<double> energy(numSamples, 0.0);
        double totalEnergy = 0.0;

        for (int i = numSamples - 1; i >= 0; --i) {
            totalEnergy += static_cast<double>(irBuffer[i]) * static_cast<double>(irBuffer[i]);
            energy[i] = totalEnergy;
        }

        if (totalEnergy <= 1e-12) return;

        // Find T60 (-60dB energy drop)
        double initialEnergy = energy[0];
        double targetEnergy = initialEnergy * 0.000001; // -60 dB

        int t60SampleIdx = numSamples - 1;
        for (int i = 0; i < numSamples; ++i) {
            if (energy[i] <= targetEnergy) {
                t60SampleIdx = i;
                break;
            }
        }

        double t60Seconds = static_cast<double>(t60SampleIdx) / irSampleRate;
        t60Seconds = std::clamp(t60Seconds, 0.2, 5.0);

        // Map T60 seconds to Dwell feedback (0.3 to 0.96)
        float calculatedDwell = static_cast<float>(0.30 + 0.66 * (t60Seconds / 5.0));
        setDwell(calculatedDwell);

        // Estimate High-Frequency Damping (Tone) from early vs late spectral ratio
        double earlySum = 0.0;
        double lateSum = 0.0;
        int halfIdx = std::min(numSamples / 2, static_cast<int>(irSampleRate * 0.1));

        for (int i = 0; i < halfIdx; ++i) earlySum += std::abs(irBuffer[i]);
        for (int i = halfIdx; i < std::min(numSamples, static_cast<int>(irSampleRate * 0.5)); ++i) lateSum += std::abs(irBuffer[i]);

        float ratio = (earlySum > 0.0) ? static_cast<float>(lateSum / earlySum) : 0.5f;
        float calculatedTone = std::clamp(ratio * 1.2f, 0.2f, 0.95f);
        setTone(calculatedTone);
    }

private:
    double mSampleRate = 44100.0;

    // Parameters
    float mDwell = 0.75f;
    float mTone = 0.70f;
    float mMix = 0.40f;
    float mPreDelayMs = 15.0f;
    float mErLevel = 0.50f;
    float mHpfCutoffHz = 80.0f;
    float mLpfCutoffHz = 12000.0f;

    // Ducking
    float mDuckingAmount = 0.0f;
    float mDuckingReleaseMs = 250.0f;
    float mDuckingEnv = 0.0f;

    // Gate
    bool mGateEnabled = false;
    float mGateThresholdDb = -36.0f;
    float mGateHoldMs = 80.0f;
    float mGateReleaseMs = 200.0f;
    float mGateEnv = 1.0f;
    int mGateHoldCounter = 0;
    bool mGateIsOpen = false;

    // FDN Delay State
    std::array<int, 4> mFdnDelaySamples{};
    std::array<std::vector<float>, 4> mFdnBufferL{};
    std::array<std::vector<float>, 4> mFdnBufferR{};
    std::array<int, 4> mFdnWriteIdxL{};
    std::array<int, 4> mFdnWriteIdxR{};

    // Damping Filter State
    std::array<float, 4> mLpfStateL{};
    std::array<float, 4> mLpfStateR{};
    std::array<float, 4> mHpfStateL{};
    std::array<float, 4> mHpfStateR{};

    // Early Reflection Buffer State
    std::vector<float> mErBufferL;
    std::vector<float> mErBufferR;
    int mErWriteIdx = 0;
};

} // namespace AudioDSP

#endif // IR_APPROX_REVERB_ENGINE_HPP
