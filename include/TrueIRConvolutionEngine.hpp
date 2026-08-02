#ifndef TRUE_IR_CONVOLUTION_ENGINE_HPP
#define TRUE_IR_CONVOLUTION_ENGINE_HPP

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <stdbool.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <iostream>

#ifndef PI_FLT
#define PI_FLT 3.14159265358979323846f
#endif

namespace AudioDSP {

constexpr int PARTITION_SIZE = 128;      // Head FIR & tail block size
constexpr int FFT_SIZE = 256;            // N = 2 * PARTITION_SIZE

class TrueIRConvolver {
public:
    TrueIRConvolver() = default;
    ~TrueIRConvolver() = default;

    void prepare(double sampleRate) {
        mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
        reset();
    }

    void reset() {
        mHeadBufferL.assign(PARTITION_SIZE, 0.0f);
        mHeadBufferR.assign(PARTITION_SIZE, 0.0f);
        mHeadWriteIdx = 0;

        mInputFifoL.assign(PARTITION_SIZE, 0.0f);
        mInputFifoR.assign(PARTITION_SIZE, 0.0f);
        mFifoWriteIdx = 0;

        mFftOverlapL.assign(PARTITION_SIZE, 0.0f);
        mFftOverlapR.assign(PARTITION_SIZE, 0.0f);

        mOutputFifoL.assign(PARTITION_SIZE, 0.0f);
        mOutputFifoR.assign(PARTITION_SIZE, 0.0f);
        mOutputReadIdx = 0;

        mDuckingEnv = 0.0f;
        mGateEnv = 1.0f;
        mGateHoldCounter = 0;
        mGateIsOpen = false;

        // Clear partition spectrum history
        int numParts = static_cast<int>(mTailPartitionsL.size());
        mXHistoryL.assign(numParts, std::vector<std::complex<float>>(FFT_SIZE, {0.0f, 0.0f}));
        mXHistoryR.assign(numParts, std::vector<std::complex<float>>(FFT_SIZE, {0.0f, 0.0f}));
        mHistoryIdx = 0;
    }

    // Parameters
    void setMix(float mix) { mMix = std::clamp(mix, 0.0f, 1.0f); }
    void setPreDelayMs(float ms) { mPreDelayMs = std::clamp(ms, 0.0f, 100.0f); }
    void setHpfCutoffHz(float hz) { mHpfCutoffHz = std::clamp(hz, 20.0f, 1000.0f); }
    void setLpfCutoffHz(float hz) { mLpfCutoffHz = std::clamp(hz, 1000.0f, 20000.0f); }

    void setDuckingAmount(float amt) { mDuckingAmount = std::clamp(amt, 0.0f, 1.0f); }
    void setDuckingReleaseMs(float ms) { mDuckingReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }

    void setGateEnabled(bool enabled) { mGateEnabled = enabled; }
    void setGateThresholdDb(float db) { mGateThresholdDb = std::clamp(db, -60.0f, 0.0f); }
    void setGateHoldMs(float ms) { mGateHoldMs = std::clamp(ms, 0.0f, 500.0f); }
    void setGateReleaseMs(float ms) { mGateReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }

    float getMix() const { return mMix; }
    float getPreDelayMs() const { return mPreDelayMs; }
    float getHpfCutoffHz() const { return mHpfCutoffHz; }
    float getLpfCutoffHz() const { return mLpfCutoffHz; }
    bool isLoaded() const { return mIsLoaded; }
    int getNumIrSamples() const { return mNumIrSamples; }
    const std::vector<float>& getRawIrWaveform() const { return mRawIrMono; }

    /**
     * @brief Load & Prune True IR File with Zero-Latency Partitioning
     * 1. Energy Decay Noise Pruning (-60dB threshold)
     * 2. Head Partition (First 128 samples) -> Direct Zero-Latency FIR
     * 3. Tail Partitions (Remaining samples) -> Partitioned Overlap-Save FFT Array
     */
    bool loadImpulseResponse(const float* irBuffer, int numSamples, double irSampleRate) {
        if (!irBuffer || numSamples <= 0 || irSampleRate <= 0.0) return false;

        // 1. Peak Amplitude & Energy Decay Pruning (-60dB threshold)
        float maxAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i) maxAbs = std::max(maxAbs, std::abs(irBuffer[i]));
        if (maxAbs <= 1e-6f) return false;

        // Find onset
        int onsetIdx = 0;
        float onsetThresh = maxAbs * 0.02f;
        for (int i = 0; i < numSamples; ++i) {
            if (std::abs(irBuffer[i]) >= onsetThresh) { onsetIdx = i; break; }
        }

        // Find prune end index (energy below -60dB)
        int pruneEndIdx = numSamples - 1;
        float noiseThresh = maxAbs * 0.001f; // -60dB
        for (int i = numSamples - 1; i >= onsetIdx; --i) {
            if (std::abs(irBuffer[i]) >= noiseThresh) {
                pruneEndIdx = i;
                break;
            }
        }

        int activeSamples = std::max(PARTITION_SIZE, pruneEndIdx - onsetIdx + 1);
        mNumIrSamples = activeSamples;
        mRawIrMono.assign(activeSamples, 0.0f);
        for (int i = 0; i < activeSamples; ++i) {
            mRawIrMono[i] = irBuffer[onsetIdx + i] / maxAbs; // Normalize
        }

        // 2. Separate Head Partition (First 128 samples for 0-latency)
        mHeadIrL.assign(PARTITION_SIZE, 0.0f);
        mHeadIrR.assign(PARTITION_SIZE, 0.0f);
        for (int i = 0; i < PARTITION_SIZE && i < activeSamples; ++i) {
            mHeadIrL[i] = mRawIrMono[i];
            mHeadIrR[i] = mRawIrMono[i];
        }

        // 3. Partition Remaining Tail into 128-sample Blocks & compute FFT
        int tailSamples = activeSamples - PARTITION_SIZE;
        int numTailPartitions = (tailSamples > 0) ? ((tailSamples + PARTITION_SIZE - 1) / PARTITION_SIZE) : 0;
        numTailPartitions = std::min(numTailPartitions, 64); // Cap at 64 blocks (~8.2 seconds)

        mTailPartitionsL.assign(numTailPartitions, std::vector<std::complex<float>>(FFT_SIZE, {0.0f, 0.0f}));
        mTailPartitionsR.assign(numTailPartitions, std::vector<std::complex<float>>(FFT_SIZE, {0.0f, 0.0f}));

        for (int p = 0; p < numTailPartitions; ++p) {
            std::vector<std::complex<float>> block(FFT_SIZE, {0.0f, 0.0f});
            int startOffset = PARTITION_SIZE + p * PARTITION_SIZE;

            for (int i = 0; i < PARTITION_SIZE; ++i) {
                if (startOffset + i < activeSamples) {
                    block[i] = { mRawIrMono[startOffset + i], 0.0f };
                }
            }

            fft256(block, false);
            mTailPartitionsL[p] = block;
            mTailPartitionsR[p] = block;
        }

        reset();
        mIsLoaded = true;
        return true;
    }

    void processSample(float inL, float inR, float& outL, float& outR) {
        if (!mIsLoaded) {
            outL = inL; outR = inR;
            return;
        }

        float inputMono = 0.5f * (inL + inR);

        // --- 1. Zero-Latency Head FIR Convolution (First 128 samples) ---
        mHeadBufferL[mHeadWriteIdx] = inputMono;
        mHeadBufferR[mHeadWriteIdx] = inputMono;

        float headOutL = 0.0f, headOutR = 0.0f;
        for (int i = 0; i < PARTITION_SIZE; ++i) {
            int readIdx = (mHeadWriteIdx - i + PARTITION_SIZE) % PARTITION_SIZE;
            headOutL += mHeadBufferL[readIdx] * mHeadIrL[i];
            headOutR += mHeadBufferR[readIdx] * mHeadIrR[i];
        }

        mHeadWriteIdx = (mHeadWriteIdx + 1) % PARTITION_SIZE;

        // --- 2. Partitioned Overlap-Save FFT Tail Convolution ---
        float tailOutL = 0.0f, tailOutR = 0.0f;
        int numParts = static_cast<int>(mTailPartitionsL.size());

        if (numParts > 0) {
            mInputFifoL[mFifoWriteIdx] = inputMono;
            mInputFifoR[mFifoWriteIdx] = inputMono;
            mFifoWriteIdx++;

            if (mFifoWriteIdx >= PARTITION_SIZE) {
                // Perform Block FFT
                std::vector<std::complex<float>> xBlockL(FFT_SIZE, {0.0f, 0.0f});
                std::vector<std::complex<float>> xBlockR(FFT_SIZE, {0.0f, 0.0f});

                for (int i = 0; i < PARTITION_SIZE; ++i) {
                    xBlockL[i] = { mFftOverlapL[i], 0.0f };
                    xBlockR[i] = { mFftOverlapR[i], 0.0f };
                    xBlockL[PARTITION_SIZE + i] = { mInputFifoL[i], 0.0f };
                    xBlockR[PARTITION_SIZE + i] = { mInputFifoR[i], 0.0f };

                    mFftOverlapL[i] = mInputFifoL[i];
                    mFftOverlapR[i] = mInputFifoR[i];
                }

                fft256(xBlockL, false);
                fft256(xBlockR, false);

                // Store in history
                mHistoryIdx = (mHistoryIdx + 1) % numParts;
                mXHistoryL[mHistoryIdx] = xBlockL;
                mXHistoryR[mHistoryIdx] = xBlockR;

                // Frequency-Domain Accumulation: Y(k) = sum( X(k, m-i) * H_i(k) )
                std::vector<std::complex<float>> yBlockL(FFT_SIZE, {0.0f, 0.0f});
                std::vector<std::complex<float>> yBlockR(FFT_SIZE, {0.0f, 0.0f});

                for (int p = 0; p < numParts; ++p) {
                    int histPos = (mHistoryIdx - p + numParts) % numParts;
                    const auto& X_L = mXHistoryL[histPos];
                    const auto& X_R = mXHistoryR[histPos];
                    const auto& H_L = mTailPartitionsL[p];
                    const auto& H_R = mTailPartitionsR[p];

                    for (int k = 0; k < FFT_SIZE; ++k) {
                        yBlockL[k] += X_L[k] * H_L[k];
                        yBlockR[k] += X_R[k] * H_R[k];
                    }
                }

                // Inverse FFT
                fft256(yBlockL, true);
                fft256(yBlockR, true);

                for (int i = 0; i < PARTITION_SIZE; ++i) {
                    mOutputFifoL[i] = yBlockL[PARTITION_SIZE + i].real();
                    mOutputFifoR[i] = yBlockR[PARTITION_SIZE + i].real();
                }

                mFifoWriteIdx = 0;
                mOutputReadIdx = 0;
            }

            tailOutL = mOutputFifoL[mOutputReadIdx];
            tailOutR = mOutputFifoR[mOutputReadIdx];
            mOutputReadIdx = (mOutputReadIdx + 1) % PARTITION_SIZE;
        }

        float wetL = headOutL + tailOutL;
        float wetR = headOutR + tailOutR;

        // --- 3. Filters (HPF / LPF) ---
        float lpfCoeff = 1.0f - std::exp(-2.0f * PI_FLT * mLpfCutoffHz / static_cast<float>(mSampleRate));
        float hpfCoeff = std::exp(-2.0f * PI_FLT * mHpfCutoffHz / static_cast<float>(mSampleRate));

        mLpfStateL += lpfCoeff * (wetL - mLpfStateL);
        mLpfStateR += lpfCoeff * (wetR - mLpfStateR);
        mHpfStateL = hpfCoeff * (mHpfStateL + mLpfStateL - wetL);
        mHpfStateR = hpfCoeff * (mHpfStateR + mLpfStateR - wetR);

        wetL = mLpfStateL - mHpfStateL;
        wetR = mLpfStateR - mHpfStateR;

        // --- 4. Ducking ---
        if (mDuckingAmount > 0.001f) {
            float inAbs = std::abs(inputMono);
            float duckAttackCoeff = 0.01f;
            float duckReleaseCoeff = 1.0f - std::exp(-1.0f / (mSampleRate * (mDuckingReleaseMs / 1000.0f)));

            if (inAbs > mDuckingEnv) mDuckingEnv += duckAttackCoeff * (inAbs - mDuckingEnv);
            else                     mDuckingEnv += duckReleaseCoeff * (inAbs - mDuckingEnv);

            float duckGain = std::clamp(1.0f - (mDuckingEnv * mDuckingAmount), 0.0f, 1.0f);
            wetL *= duckGain; wetR *= duckGain;
        }

        // --- 5. Noise Gate ---
        if (mGateEnabled) {
            float wetAbs = std::max(std::abs(wetL), std::abs(wetR));
            float threshLin = std::pow(10.0f, mGateThresholdDb / 20.0f);
            int holdSamples = static_cast<int>(mSampleRate * (mGateHoldMs / 1000.0f));
            float relCoeff = std::exp(-1.0f / (mSampleRate * (mGateReleaseMs / 1000.0f)));

            if (wetAbs > threshLin) {
                mGateIsOpen = true; mGateHoldCounter = holdSamples; mGateEnv = 1.0f;
            } else {
                if (mGateHoldCounter > 0) { mGateHoldCounter--; mGateEnv = 1.0f; }
                else { mGateEnv *= relCoeff; }
            }

            wetL *= mGateEnv; wetR *= mGateEnv;
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

    // Export Slim Pruned WAV File for Hardware Pedals
    bool exportSlimWavFile(const std::string& filepath) {
        if (mRawIrMono.empty()) return false;

        std::ofstream outFile(filepath, std::ios::binary);
        if (!outFile.is_open()) return false;

        uint32_t sampleRate = static_cast<uint32_t>(mSampleRate);
        uint16_t numChannels = 1;
        uint16_t bitsPerSample = 16;
        uint32_t numSamples = static_cast<uint32_t>(mRawIrMono.size());
        uint32_t dataChunkSize = numSamples * (bitsPerSample / 8);
        uint32_t fileSize = 36 + dataChunkSize;

        // RIFF Header
        outFile.write("RIFF", 4);
        outFile.write(reinterpret_cast<const char*>(&fileSize), 4);
        outFile.write("WAVE", 4);

        // fmt Chunk
        outFile.write("fmt ", 4);
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        uint16_t blockAlign = numChannels * (bitsPerSample / 8);

        outFile.write(reinterpret_cast<const char*>(&fmtSize), 4);
        outFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
        outFile.write(reinterpret_cast<const char*>(&numChannels), 2);
        outFile.write(reinterpret_cast<const char*>(&sampleRate), 4);
        outFile.write(reinterpret_cast<const char*>(&byteRate), 4);
        outFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
        outFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

        // data Chunk
        outFile.write("data", 4);
        outFile.write(reinterpret_cast<const char*>(&dataChunkSize), 4);

        for (float s : mRawIrMono) {
            int16_t pcmVal = static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
            outFile.write(reinterpret_cast<const char*>(&pcmVal), 2);
        }

        return true;
    }

private:
    // Cooley-Tukey Radix-2 FFT (N = 256)
    static void fft256(std::vector<std::complex<float>>& x, bool inverse) {
        constexpr int N = FFT_SIZE;
        // Bit-reversal permutation
        for (int i = 1, j = 0; i < N; ++i) {
            int bit = N >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(x[i], x[j]);
        }

        for (int len = 2; len <= N; len <<= 1) {
            float angle = 2.0f * PI_FLT / len * (inverse ? 1.0f : -1.0f);
            std::complex<float> wlen(std::cos(angle), std::sin(angle));
            for (int i = 0; i < N; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for (int j = 0; j < len / 2; ++j) {
                    std::complex<float> u = x[i + j];
                    std::complex<float> v = x[i + j + len / 2] * w;
                    x[i + j] = u + v;
                    x[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse) {
            for (int i = 0; i < N; ++i) {
                x[i] /= static_cast<float>(N);
            }
        }
    }

private:
    double mSampleRate = 44100.0;
    bool mIsLoaded = false;
    int mNumIrSamples = 0;

    std::vector<float> mRawIrMono;

    // Head Zero-Latency FIR
    std::vector<float> mHeadIrL;
    std::vector<float> mHeadIrR;
    std::vector<float> mHeadBufferL;
    std::vector<float> mHeadBufferR;
    int mHeadWriteIdx = 0;

    // Tail Partitioned FFT
    std::vector<std::vector<std::complex<float>>> mTailPartitionsL;
    std::vector<std::vector<std::complex<float>>> mTailPartitionsR;
    std::vector<std::vector<std::complex<float>>> mXHistoryL;
    std::vector<std::vector<std::complex<float>>> mXHistoryR;
    int mHistoryIdx = 0;

    std::vector<float> mInputFifoL;
    std::vector<float> mInputFifoR;
    int mFifoWriteIdx = 0;

    std::vector<float> mFftOverlapL;
    std::vector<float> mFftOverlapR;

    std::vector<float> mOutputFifoL;
    std::vector<float> mOutputFifoR;
    int mOutputReadIdx = 0;

    // Parameters
    float mMix = 0.40f;
    float mPreDelayMs = 0.0f;
    float mHpfCutoffHz = 40.0f;
    float mLpfCutoffHz = 16000.0f;

    float mLpfStateL = 0.0f, mLpfStateR = 0.0f;
    float mHpfStateL = 0.0f, mHpfStateR = 0.0f;

    // Ducking & Gate
    float mDuckingAmount = 0.0f;
    float mDuckingReleaseMs = 250.0f;
    float mDuckingEnv = 0.0f;

    bool mGateEnabled = false;
    float mGateThresholdDb = -36.0f;
    float mGateHoldMs = 80.0f;
    float mGateReleaseMs = 200.0f;
    float mGateEnv = 1.0f;
    int mGateHoldCounter = 0;
    bool mGateIsOpen = false;
};

} // namespace AudioDSP

#endif // TRUE_IR_CONVOLUTION_ENGINE_HPP
