#include "../include/HarmonicTremoloEngine.hpp"
#include "../embedded/mooer_harmonic_tremolo.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

int main() {
    std::cout << "========================================================\n";
    std::cout << " Running Harmonic Tremolo DSP Engine & Embedded Tests...\n";
    std::cout << "========================================================\n\n";

    // 1. Test C++ Engine Initialization and Prepare
    AudioDSP::HarmonicTremoloEngine cppEngine;
    cppEngine.prepare(48000.0);
    cppEngine.setRate(4.0f);
    cppEngine.setDepth(0.9f);
    cppEngine.setCrossoverFrequency(750.0f);
    cppEngine.setWarmth(0.5f);
    cppEngine.setStereoPhaseOffset(90.0f);

    std::cout << "[PASS] C++ DSP Engine initialized successfully.\n";

    // 2. Test Processing Audio Buffer without NaNs or Infinities
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

    std::cout << "[PASS] C++ DSP Block Process executed 5,120 samples cleanly without NaNs/Infs.\n";

    // 3. Test Embedded C Implementation
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

    std::cout << "[PASS] Embedded C DSP Process executed 1,024 samples cleanly without NaNs/Infs.\n";
    std::cout << "\nAll DSP math tests completed successfully!\n";

    return 0;
}
