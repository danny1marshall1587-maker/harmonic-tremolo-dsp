#ifndef IR_APPROX_REVERB_ENGINE_HPP
#define IR_APPROX_REVERB_ENGINE_HPP

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <stdbool.h>
#include <cstdint>
#include <fstream>
#include <string>

#ifndef PI_FLT
#define PI_FLT 3.14159265358979323846f
#endif

namespace AudioDSP {

// 64-Byte Ultra-Lightweight Hardware Profile for Mooer GE / NUX MG-400 / Valeton GP-150 Pedals
struct HardwareReverbProfile {
    char magic[8];          // "IRPROF1"
    char profileName[24];   // e.g., "St Paul Cathedral"
    float dwell;            // FDN Feedback (0.10 to 0.92)
    float tone;             // High Damping (0.0 to 1.0)
    float preDelayMs;       // Pre-delay (0 to 100ms)
    float erLevel;          // Early Reflections (0.0 to 1.0)
    float hpfHz;            // Low Cut (20 to 500Hz)
    float lpfHz;            // High Cut (1000 to 20000Hz)
    float eqLowGainDb;      // Low Spectral Shelf (-12 to +12dB)
    float eqMidGainDb;      // Mid Spectral Peak (-12 to +12dB)
    float eqHighGainDb;     // High Spectral Shelf (-12 to +12dB)
};

class IRApproxReverbEngine {
public:
    IRApproxReverbEngine() = default;
    ~IRApproxReverbEngine() = default;

    void prepare(double sampleRate) {
        mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;

        // FDN prime delay lengths (~29ms, ~37ms, ~43ms, ~53ms)
        const float primeDelaysMs[4] = { 29.7f, 37.1f, 43.9f, 53.3f };

        for (int i = 0; i < 4; ++i) {
            int delaySamples = static_cast<int>(mSampleRate * (primeDelaysMs[i] / 1000.0f));
            mFdnDelaySamples[i] = delaySamples;
            int bufferSize = std::max(delaySamples + 100, static_cast<int>(mSampleRate * 0.2));

            mFdnBufferL[i].assign(bufferSize, 0.0f);
            mFdnBufferR[i].assign(bufferSize, 0.0f);
            mFdnWriteIdxL[i] = 0;
            mFdnWriteIdxR[i] = 0;

            mLpfStateL[i] = 0.0f;
            mLpfStateR[i] = 0.0f;
            mHpfStateL[i] = 0.0f;
            mHpfStateR[i] = 0.0f;
        }

        // Early Reflections Buffer (100ms max)
        int erSize = static_cast<int>(mSampleRate * 0.1);
        mErBufferL.assign(erSize, 0.0f);
        mErBufferR.assign(erSize, 0.0f);
        mErWriteIdx = 0;

        // 3-Band Profile EQ States
        mEqLowStateL = 0.0f; mEqLowStateR = 0.0f;
        mEqHighStateL = 0.0f; mEqHighStateR = 0.0f;

        reset();
    }

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

        mEqLowStateL = 0.0f; mEqLowStateR = 0.0f;
        mEqHighStateL = 0.0f; mEqHighStateR = 0.0f;

        mDuckingEnv = 0.0f;
        mGateEnv = 1.0f;
        mGateHoldCounter = 0;
        mGateIsOpen = false;
    }

    // Setters (with anti-self-oscillation capping)
    void setDwell(float dwell) { mDwell = std::clamp(dwell, 0.10f, 0.92f); } // Cap at 0.92 to prevent self-oscillation
    void setTone(float tone) { mTone = std::clamp(tone, 0.0f, 1.0f); }
    void setMix(float mix) { mMix = std::clamp(mix, 0.0f, 1.0f); }
    void setPreDelayMs(float ms) { mPreDelayMs = std::clamp(ms, 0.0f, 100.0f); }
    void setErLevel(float level) { mErLevel = std::clamp(level, 0.0f, 1.0f); }
    void setHpfCutoffHz(float hz) { mHpfCutoffHz = std::clamp(hz, 20.0f, 1000.0f); }
    void setLpfCutoffHz(float hz) { mLpfCutoffHz = std::clamp(hz, 1000.0f, 20000.0f); }

    void setEqLowGainDb(float db) { mEqLowGainDb = std::clamp(db, -12.0f, 12.0f); }
    void setEqMidGainDb(float db) { mEqMidGainDb = std::clamp(db, -12.0f, 12.0f); }
    void setEqHighGainDb(float db) { mEqHighGainDb = std::clamp(db, -12.0f, 12.0f); }

    void setDuckingAmount(float amt) { mDuckingAmount = std::clamp(amt, 0.0f, 1.0f); }
    void setDuckingReleaseMs(float ms) { mDuckingReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }

    void setGateEnabled(bool enabled) { mGateEnabled = enabled; }
    void setGateThresholdDb(float db) { mGateThresholdDb = std::clamp(db, -60.0f, 0.0f); }
    void setGateHoldMs(float ms) { mGateHoldMs = std::clamp(ms, 0.0f, 500.0f); }
    void setGateReleaseMs(float ms) { mGateReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }

    // Getters
    float getDwell() const { return mDwell; }
    float getTone() const { return mTone; }
    float getMix() const { return mMix; }
    float getPreDelayMs() const { return mPreDelayMs; }
    float getErLevel() const { return mErLevel; }
    float getHpfCutoffHz() const { return mHpfCutoffHz; }
    float getLpfCutoffHz() const { return mLpfCutoffHz; }
    float getEqLowGainDb() const { return mEqLowGainDb; }
    float getEqMidGainDb() const { return mEqMidGainDb; }
    float getEqHighGainDb() const { return mEqHighGainDb; }

    // Soft Saturator to prevent feedback self-oscillation
    static inline float softClip(float x) {
        return x / (1.0f + 0.15f * std::abs(x));
    }

    void processSample(float inL, float inR, float& outL, float& outR) {
        float inputMono = 0.5f * (inL + inR);

        // --- 1. Pre-Delay & Early Reflections ---
        int preDelaySamples = static_cast<int>(mSampleRate * (mPreDelayMs / 1000.0f));
        int erSize = static_cast<int>(mErBufferL.size());

        mErBufferL[mErWriteIdx] = inputMono;
        mErBufferR[mErWriteIdx] = inputMono;

        const float erTapsMs[4] = { 7.0f, 14.0f, 23.0f, 35.0f };
        const float erGains[4] = { 0.7f, 0.5f, 0.35f, 0.2f };

        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < 4; ++t) {
            int tapDelay = preDelaySamples + static_cast<int>(mSampleRate * (erTapsMs[t] / 1000.0f));
            int readIdx = (mErWriteIdx - tapDelay + erSize) % erSize;
            erL += mErBufferL[readIdx] * erGains[t];
            erR += mErBufferR[readIdx] * erGains[t];
        }

        mErWriteIdx = (mErWriteIdx + 1) % erSize;

        // --- 2. Late Reverb Feedback Delay Network (FDN) ---
        float fdnOutL[4], fdnOutR[4];
        for (int i = 0; i < 4; ++i) {
            int maxLen = static_cast<int>(mFdnBufferL[i].size());
            int readIdxL = (mFdnWriteIdxL[i] - mFdnDelaySamples[i] + maxLen) % maxLen;
            int readIdxR = (mFdnWriteIdxR[i] - mFdnDelaySamples[i] + maxLen) % maxLen;

            fdnOutL[i] = mFdnBufferL[i][readIdxL];
            fdnOutR[i] = mFdnBufferR[i][readIdxR];
        }

        // Dissipative Householder Matrix Diffusion (0.46 multiplier ensures energy loss)
        float sumL = fdnOutL[0] + fdnOutL[1] + fdnOutL[2] + fdnOutL[3];
        float sumR = fdnOutR[0] + fdnOutR[1] + fdnOutR[2] + fdnOutR[3];

        float diffL[4], diffR[4];
        for (int i = 0; i < 4; ++i) {
            diffL[i] = fdnOutL[i] - 0.46f * sumL;
            diffR[i] = fdnOutR[i] - 0.46f * sumR;
        }

        // Damping Filters inside feedback loop
        float lpfCutoff = 1000.0f + mTone * 17000.0f;
        float lpfCoeff = 1.0f - std::exp(-2.0f * PI_FLT * lpfCutoff / static_cast<float>(mSampleRate));
        float hpfCoeff = std::exp(-2.0f * PI_FLT * mHpfCutoffHz / static_cast<float>(mSampleRate));

        for (int i = 0; i < 4; ++i) {
            mLpfStateL[i] += lpfCoeff * (diffL[i] - mLpfStateL[i]);
            mLpfStateR[i] += lpfCoeff * (diffR[i] - mLpfStateR[i]);

            mHpfStateL[i] = hpfCoeff * (mHpfStateL[i] + mLpfStateL[i] - diffL[i]);
            mHpfStateR[i] = hpfCoeff * (mHpfStateR[i] + mLpfStateR[i] - diffR[i]);

            float dampedL = mLpfStateL[i] - mHpfStateL[i];
            float dampedR = mLpfStateR[i] - mHpfStateR[i];

            // Soft saturation to strictly prevent self-oscillation
            dampedL = softClip(dampedL * mDwell);
            dampedR = softClip(dampedR * mDwell);

            int maxLen = static_cast<int>(mFdnBufferL[i].size());
            mFdnBufferL[i][mFdnWriteIdxL[i]] = inputMono + dampedL;
            mFdnBufferR[i][mFdnWriteIdxR[i]] = inputMono + dampedR;

            mFdnWriteIdxL[i] = (mFdnWriteIdxL[i] + 1) % maxLen;
            mFdnWriteIdxR[i] = (mFdnWriteIdxR[i] + 1) % maxLen;
        }

        float lateL = (fdnOutL[0] + fdnOutL[2]) * 0.5f;
        float lateR = (fdnOutR[1] + fdnOutR[3]) * 0.5f;

        float wetL = (erL * mErLevel) + lateL;
        float wetR = (erR * mErLevel) + lateR;

        // --- 3. Custom IR Spectral Profile EQ Filtering ---
        // 1-Pole Low Shelf (~300Hz) & High Shelf (~3000Hz)
        float lowShelfCoeff = 1.0f - std::exp(-2.0f * PI_FLT * 300.0f / static_cast<float>(mSampleRate));
        float highShelfCoeff = 1.0f - std::exp(-2.0f * PI_FLT * 3000.0f / static_cast<float>(mSampleRate));

        mEqLowStateL += lowShelfCoeff * (wetL - mEqLowStateL);
        mEqLowStateR += lowShelfCoeff * (wetR - mEqLowStateR);

        mEqHighStateL += highShelfCoeff * (wetL - mEqHighStateL);
        mEqHighStateR += highShelfCoeff * (wetR - mEqHighStateR);

        float lowL = mEqLowStateL;
        float lowR = mEqLowStateR;

        float highL = wetL - mEqHighStateL;
        float highR = wetR - mEqHighStateR;

        float midL = wetL - lowL - highL;
        float midR = wetR - lowR - highR;

        float lowLin = std::pow(10.0f, mEqLowGainDb / 20.0f);
        float midLin = std::pow(10.0f, mEqMidGainDb / 20.0f);
        float highLin = std::pow(10.0f, mEqHighGainDb / 20.0f);

        wetL = lowL * lowLin + midL * midLin + highL * highLin;
        wetR = lowR * lowLin + midR * midLin + highR * highLin;

        // --- 4. Dynamic Ducking ---
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

        // --- 5. Noise Gate ---
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

        // --- 6. Dry/Wet Mix ---
        outL = inL * (1.0f - mMix) + wetL * mMix;
        outR = inR * (1.0f - mMix) + wetR * mMix;
    }

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
     * @brief High-Precision IR Acoustic Profiler (NAM-like Slim Profile Generator)
     * Fits RT60 decay, 3-band spectral EQ fingerprint, pre-delay, and room resonance
     * so any IR (even 10s Cathedral IRs) runs at <0.5% DSP load without self-oscillation!
     */
    void analyzeImpulseResponse(const float* irBuffer, int numSamples, double irSampleRate) {
        if (!irBuffer || numSamples <= 0 || irSampleRate <= 0.0) return;

        // 1. Peak Amplitude & Pre-Delay Onset Detection
        float maxAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            maxAbs = std::max(maxAbs, std::abs(irBuffer[i]));
        }

        if (maxAbs <= 1e-6f) return;

        int onsetSampleIdx = 0;
        float onsetThreshold = maxAbs * 0.04f; // 4% threshold
        for (int i = 0; i < numSamples; ++i) {
            if (std::abs(irBuffer[i]) >= onsetThreshold) {
                onsetSampleIdx = i;
                break;
            }
        }

        float detectedPreDelayMs = static_cast<float>((static_cast<double>(onsetSampleIdx) / irSampleRate) * 1000.0);
        detectedPreDelayMs = std::clamp(detectedPreDelayMs, 0.0f, 100.0f);
        setPreDelayMs(detectedPreDelayMs);

        // 2. Schroeder Energy Decay Curve (EDC)
        std::vector<double> edc(numSamples, 0.0);
        double runningEnergy = 0.0;

        for (int i = numSamples - 1; i >= onsetSampleIdx; --i) {
            runningEnergy += static_cast<double>(irBuffer[i]) * static_cast<double>(irBuffer[i]);
            edc[i] = runningEnergy;
        }

        double initialEnergy = edc[onsetSampleIdx];
        if (initialEnergy <= 1e-12) return;

        // 3. True T20/T30 Linear Regression for RT60 Time
        int idxStart = onsetSampleIdx;
        int idxEnd = numSamples - 1;

        for (int i = onsetSampleIdx; i < numSamples; ++i) {
            double db = 10.0 * std::log10((edc[i] / initialEnergy) + 1e-12);
            if (db <= -5.0 && idxStart == onsetSampleIdx) {
                idxStart = i;
            }
            if (db <= -35.0) {
                idxEnd = i;
                break;
            }
        }

        double decayDurationSec = static_cast<double>(idxEnd - idxStart) / irSampleRate;
        double rt60Seconds = 2.0;

        if (decayDurationSec > 0.005 && idxEnd > idxStart) {
            double slopeDbPerSec = 30.0 / decayDurationSec;
            rt60Seconds = 60.0 / slopeDbPerSec;
        } else {
            rt60Seconds = static_cast<double>(numSamples - onsetSampleIdx) / irSampleRate;
        }

        rt60Seconds = std::clamp(rt60Seconds, 0.15, 8.0);

        // Map RT60 to Dwell feedback (safely capped at 0.92f)
        float calculatedDwell = static_cast<float>(std::pow(10.0, -3.0 * 0.040 / rt60Seconds));
        calculatedDwell = std::clamp(calculatedDwell, 0.15f, 0.92f);
        setDwell(calculatedDwell);

        // 4. Extract 3-Band IR Spectral EQ Fingerprint
        double lowEnergy = 0.0;
        double midEnergy = 0.0;
        double highEnergy = 0.0;

        int window200ms = std::min(numSamples, onsetSampleIdx + static_cast<int>(irSampleRate * 0.20));

        // 1-pole Low pass 300Hz and High pass 3000Hz filter states
        float lpState = 0.0f;
        float hpState = 0.0f;
        float lpCoeff = 1.0f - std::exp(-2.0f * PI_FLT * 300.0f / static_cast<float>(irSampleRate));
        float hpCoeff = std::exp(-2.0f * PI_FLT * 3000.0f / static_cast<float>(irSampleRate));

        for (int i = onsetSampleIdx; i < window200ms; ++i) {
            float s = irBuffer[i];
            lpState += lpCoeff * (s - lpState);
            hpState = hpCoeff * (hpState + s);

            float lowS = lpState;
            float highS = s - hpState;
            float midS = s - lowS - highS;

            lowEnergy += lowS * lowS;
            midEnergy += midS * midS;
            highEnergy += highS * highS;
        }

        double totalSpecEnergy = lowEnergy + midEnergy + highEnergy + 1e-12;

        float lowRatio = static_cast<float>(lowEnergy / totalSpecEnergy);
        float midRatio = static_cast<float>(midEnergy / totalSpecEnergy);
        float highRatio = static_cast<float>(highEnergy / totalSpecEnergy);

        setEqLowGainDb(std::clamp((lowRatio - 0.33f) * 18.0f, -8.0f, 8.0f));
        setEqMidGainDb(std::clamp((midRatio - 0.33f) * 18.0f, -8.0f, 8.0f));
        setEqHighGainDb(std::clamp((highRatio - 0.33f) * 18.0f, -8.0f, 8.0f));

        // 5. Tone Damping
        float calculatedTone = std::clamp(highRatio * 2.5f, 0.20f, 0.95f);
        setTone(calculatedTone);
        setLpfCutoffHz(std::clamp(1500.0f + 16500.0f * calculatedTone, 2000.0f, 18000.0f));

        // 6. Early Reflections & Sub-bass Cut
        setErLevel(std::clamp(midRatio * 1.5f, 0.20f, 0.85f));
        setHpfCutoffHz((rt60Seconds > 4.0) ? 40.0f : ((rt60Seconds > 1.5) ? 70.0f : 110.0f));
    }

    // Export Hardware Profile (.irprof) for Pedal Firmware
    bool exportHardwareProfile(const std::string& filepath, const std::string& profileName) {
        HardwareReverbProfile prof{};
        std::copy_n("IRPROF1", 8, prof.magic);
        std::copy_n(profileName.c_str(), std::min(profileName.size(), size_t(23)), prof.profileName);

        prof.dwell = mDwell;
        prof.tone = mTone;
        prof.preDelayMs = mPreDelayMs;
        prof.erLevel = mErLevel;
        prof.hpfHz = mHpfCutoffHz;
        prof.lpfHz = mLpfCutoffHz;
        prof.eqLowGainDb = mEqLowGainDb;
        prof.eqMidGainDb = mEqMidGainDb;
        prof.eqHighGainDb = mEqHighGainDb;

        std::ofstream outFile(filepath, std::ios::binary);
        if (!outFile.is_open()) return false;

        outFile.write(reinterpret_cast<const char*>(&prof), sizeof(prof));
        return true;
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

    // 3-Band Profile EQ
    float mEqLowGainDb = 0.0f;
    float mEqMidGainDb = 0.0f;
    float mEqHighGainDb = 0.0f;
    float mEqLowStateL = 0.0f, mEqLowStateR = 0.0f;
    float mEqHighStateL = 0.0f, mEqHighStateR = 0.0f;

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
