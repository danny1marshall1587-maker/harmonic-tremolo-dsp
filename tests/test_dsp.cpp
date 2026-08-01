#include "../include/HarmonicTremoloEngine.hpp"
#include "../embedded/mooer_harmonic_tremolo.h"
#include "../include/IRApproxReverbEngine.hpp"
#include "../embedded/mooer_ir_reverb.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

int main() {
    std::cout << "========================================================\n";
    std::cout << " Running Tremolo & Reverb DSP Engine Tests...\n";
    std::cout << "========================================================\n\n";

    // 1. Test Tremolo C++ Engine Initialization and Prepare
    AudioDSP::HarmonicTremoloEngine cppEngine;
    cppEngine.prepare(48000.0);
    cppEngine.setRate(4.0f);
    cppEngine.setDepth(0.9f);
    cppEngine.setCrossoverFrequency(750.0f);
    cppEngine.setWarmth(0.5f);
    cppEngine.setStereoPhaseOffset(90.0f);

    std::cout << "[PASS] Tremolo C++ DSP Engine initialized successfully.\n";

    // 2. Test Processing Tremolo Audio Buffer
    constexpr int blockSize = 512;
    std::vector<float> inL(blockSize, 0.5f);
    std::vector<float> inR(blockSize, -0.5f);
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    float* inPointers[2] = { inL.data(), inR.data() };
    float* outPointers[2] = { outL.data(), outR.data() };

    for (int b = 0; b < 10; ++b) {
        cppEngine.processBlock(inPointers, outPointers, 2, blockSize);
        for (int i = 0; i < blockSize; ++i) {
            assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
            assert(!std::isnan(outR[i]) && !std::isinf(outR[i]));
        }
    }

    std::cout << "[PASS] Tremolo C++ DSP Block Process executed 5,120 samples cleanly.\n";

    // 3. Test Embedded C Tremolo
    MooerHarmonicTremolo cHandle;
    MooerHarmonicTremolo_Init(&cHandle, 48000.0f);
    cHandle.rateHz = 5.0f;
    cHandle.depth = 1.0f;
    cHandle.crossoverHz = 800.0f;
    cHandle.warmth = 0.4f;

    std::vector<float> cBuffer(1024, 0.707f);
    MooerHarmonicTremolo_ProcessBuffer(&cHandle, cBuffer.data(), static_cast<int>(cBuffer.size()));

    for (size_t i = 0; i < cBuffer.size(); ++i) {
        assert(!std::isnan(cBuffer[i]) && !std::isinf(cBuffer[i]));
    }

    std::cout << "[PASS] Embedded C Tremolo Process executed 1,024 samples cleanly.\n";

    // 4. Test Reverb C++ Engine & IR Analyzer
    AudioDSP::IRApproxReverbEngine reverbEngine;
    reverbEngine.prepare(48000.0);
    reverbEngine.setDwell(0.80f);
    reverbEngine.setTone(0.70f);
    reverbEngine.setMix(0.50f);
    reverbEngine.setDuckingAmount(0.60f);
    reverbEngine.setGateEnabled(true);

    // Mock IR Buffer (decaying noise burst)
    std::vector<float> mockIr(48000, 0.0f);
    for (size_t i = 0; i < mockIr.size(); ++i) {
        mockIr[i] = std::exp(-static_cast<float>(i) / 4800.0f) * ((i % 2 == 0) ? 0.5f : -0.5f);
    }
    reverbEngine.analyzeImpulseResponse(mockIr.data(), static_cast<int>(mockIr.size()), 48000.0);

    for (int b = 0; b < 10; ++b) {
        reverbEngine.processBlock(inPointers, outPointers, 2, blockSize);
        for (int i = 0; i < blockSize; ++i) {
            assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
            assert(!std::isnan(outR[i]) && !std::isinf(outR[i]));
        }
    }

    std::cout << "[PASS] Reverb C++ Engine & IR Analyzer executed 5,120 samples cleanly.\n";

    // 5. Test Embedded C Reverb
    MooerIRReverb cReverbHandle;
    MooerIRReverb_Init(&cReverbHandle, 48000.0f);
    cReverbHandle.dwell = 0.85f;
    cReverbHandle.tone = 0.75f;
    cReverbHandle.mix = 0.50f;

    std::vector<float> cReverbBuffer(1024, 0.5f);
    MooerIRReverb_ProcessBuffer(&cReverbHandle, cReverbBuffer.data(), static_cast<int>(cReverbBuffer.size()));

    for (size_t i = 0; i < cReverbBuffer.size(); ++i) {
        assert(!std::isnan(cReverbBuffer[i]) && !std::isinf(cReverbBuffer[i]));
    }

    std::cout << "[PASS] Embedded C Reverb executed 1,024 samples cleanly.\n";
    std::cout << "\nAll Tremolo & Reverb DSP tests completed successfully!\n";

    return 0;
}

