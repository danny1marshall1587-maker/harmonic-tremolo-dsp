#include "mooer_true_ir_convolver.h"
#include <math.h>
#include <string.h>

static inline float clampf(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

void MooerTrueIRConvolver_Init(MooerTrueIRConvolver* handle, float sampleRate) {
    if (!handle) return;
    handle->sampleRate = (sampleRate > 0.0f) ? sampleRate : 44100.0f;
    handle->mix = 0.40f;
    handle->hpfHz = 40.0f;
    handle->lpfHz = 16000.0f;

    MooerTrueIRConvolver_Reset(handle);
}

void MooerTrueIRConvolver_Reset(MooerTrueIRConvolver* handle) {
    if (!handle) return;
    memset(handle->headBuffer, 0, sizeof(handle->headBuffer));
    memset(handle->headIR, 0, sizeof(handle->headIR));
    handle->headWriteIdx = 0;

    memset(handle->tailBuffer, 0, sizeof(handle->tailBuffer));
    memset(handle->tailIR, 0, sizeof(handle->tailIR));
    handle->tailWriteIdx = 0;
    handle->tailLength = 0;

    handle->lpfState = 0.0f;
    handle->hpfState = 0.0f;
}

bool MooerTrueIRConvolver_LoadIR(MooerTrueIRConvolver* handle, const float* irData, int numSamples) {
    if (!handle || !irData || numSamples <= 0) return false;

    MooerTrueIRConvolver_Reset(handle);

    // 1. Peak normalization
    float maxAbs = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float absV = (float)fabs(irData[i]);
        if (absV > maxAbs) maxAbs = absV;
    }
    if (maxAbs <= 1e-6f) return false;

    // 2. Fill zero-latency head IR (First 128 samples)
    int headCount = (numSamples < MOOER_IR_HEAD_SIZE) ? numSamples : MOOER_IR_HEAD_SIZE;
    for (int i = 0; i < headCount; ++i) {
        handle->headIR[i] = irData[i] / maxAbs;
    }

    // 3. Fill tail IR
    int tailCount = numSamples - MOOER_IR_HEAD_SIZE;
    if (tailCount > MOOER_IR_MAX_TAIL) tailCount = MOOER_IR_MAX_TAIL;

    if (tailCount > 0) {
        for (int i = 0; i < tailCount; ++i) {
            handle->tailIR[i] = irData[MOOER_IR_HEAD_SIZE + i] / maxAbs;
        }
        handle->tailLength = tailCount;
    }

    return true;
}

float MooerTrueIRConvolver_ProcessSample(MooerTrueIRConvolver* handle, float inSample) {
    if (!handle) return inSample;

    // Zero-latency Head FIR
    handle->headBuffer[handle->headWriteIdx] = inSample;

    float headOut = 0.0f;
    for (int i = 0; i < MOOER_IR_HEAD_SIZE; ++i) {
        int readIdx = (handle->headWriteIdx - i + MOOER_IR_HEAD_SIZE) % MOOER_IR_HEAD_SIZE;
        headOut += handle->headBuffer[readIdx] * handle->headIR[i];
    }
    handle->headWriteIdx = (handle->headWriteIdx + 1) % MOOER_IR_HEAD_SIZE;

    // Tail Convolution (Sparse Time-Domain Decimated for Low DSP Load)
    float tailOut = 0.0f;
    if (handle->tailLength > 0) {
        handle->tailBuffer[handle->tailWriteIdx] = inSample;

        for (int i = 0; i < handle->tailLength; i += 2) { // 2x Decimation for ultra-low DSP CPU load
            int readIdx = (handle->tailWriteIdx - i + MOOER_IR_MAX_TAIL) % MOOER_IR_MAX_TAIL;
            tailOut += handle->tailBuffer[readIdx] * handle->tailIR[i];
        }
        handle->tailWriteIdx = (handle->tailWriteIdx + 1) % MOOER_IR_MAX_TAIL;
    }

    float wet = headOut + tailOut;

    // Filters
    float lpfCoeff = 1.0f - (float)exp(-2.0 * 3.141592653589793 * (double)handle->lpfHz / (double)handle->sampleRate);
    float hpfCoeff = (float)exp(-2.0 * 3.141592653589793 * (double)handle->hpfHz / (double)handle->sampleRate);

    handle->lpfState += lpfCoeff * (wet - handle->lpfState);
    handle->hpfState = hpfCoeff * (handle->hpfState + handle->lpfState - wet);
    wet = handle->lpfState - handle->hpfState;

    return inSample * (1.0f - handle->mix) + wet * handle->mix;
}

void MooerTrueIRConvolver_ProcessBuffer(MooerTrueIRConvolver* handle, float* buffer, int numSamples) {
    if (!handle || !buffer) return;
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = MooerTrueIRConvolver_ProcessSample(handle, buffer[i]);
    }
}
