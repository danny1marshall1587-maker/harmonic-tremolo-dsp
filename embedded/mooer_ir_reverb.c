#include "mooer_ir_reverb.h"
#include <math.h>
#include <string.h>

static inline float clampf(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

void MooerIRReverb_Init(MooerIRReverb* handle, float sampleRate) {
    if (!handle) return;
    handle->sampleRate = (sampleRate > 0.0f) ? sampleRate : 44100.0f;
    handle->dwell = 0.75f;
    handle->tone = 0.70f;
    handle->mix = 0.40f;

    // FDN prime delay lines (~29ms, ~37ms, ~43ms, ~53ms)
    handle->delaySamples[0] = (int)(handle->sampleRate * 0.0297f);
    handle->delaySamples[1] = (int)(handle->sampleRate * 0.0371f);
    handle->delaySamples[2] = (int)(handle->sampleRate * 0.0439f);
    handle->delaySamples[3] = (int)(handle->sampleRate * 0.0533f);

    for (int i = 0; i < 4; ++i) {
        if (handle->delaySamples[i] >= MOOER_REVERB_MAX_DELAY) {
            handle->delaySamples[i] = MOOER_REVERB_MAX_DELAY - 1;
        }
    }

    MooerIRReverb_Reset(handle);
}

void MooerIRReverb_Reset(MooerIRReverb* handle) {
    if (!handle) return;
    memset(handle->buffer0, 0, sizeof(handle->buffer0));
    memset(handle->buffer1, 0, sizeof(handle->buffer1));
    memset(handle->buffer2, 0, sizeof(handle->buffer2));
    memset(handle->buffer3, 0, sizeof(handle->buffer3));

    for (int i = 0; i < 4; ++i) {
        handle->writeIdx[i] = 0;
        handle->lpfState[i] = 0.0f;
        handle->hpfState[i] = 0.0f;
    }

    handle->smoothedDwell = handle->dwell;
    handle->smoothedTone = handle->tone;
    handle->smoothedMix = handle->mix;
}

void MooerIRReverb_LoadPreset(MooerIRReverb* handle, const MooerIRReverbPreset* preset) {
    if (!handle || !preset) return;
    handle->dwell = clampf(preset->dwell, 0.1f, 0.98f);
    handle->tone = clampf(preset->tone, 0.0f, 1.0f);
    handle->mix = clampf(preset->mix, 0.0f, 1.0f);
}

float MooerIRReverb_ProcessSample(MooerIRReverb* handle, float inSample) {
    if (!handle) return inSample;

    // Smooth parameters
    handle->smoothedDwell += 0.005f * (handle->dwell - handle->smoothedDwell);
    handle->smoothedTone += 0.005f * (handle->tone - handle->smoothedTone);
    handle->smoothedMix += 0.005f * (handle->mix - handle->smoothedMix);

    float* bufs[4] = { handle->buffer0, handle->buffer1, handle->buffer2, handle->buffer3 };

    // Read FDN outputs
    float fdnOut[4];
    for (int i = 0; i < 4; ++i) {
        int readIdx = (handle->writeIdx[i] - handle->delaySamples[i] + MOOER_REVERB_MAX_DELAY) % MOOER_REVERB_MAX_DELAY;
        fdnOut[i] = bufs[i][readIdx];
    }

    // 4x4 Householder Unitary Diffusion: y_i = x_i - 0.5 * sum(x)
    float sum = fdnOut[0] + fdnOut[1] + fdnOut[2] + fdnOut[3];
    float diff[4];
    for (int i = 0; i < 4; ++i) {
        diff[i] = fdnOut[i] - 0.5f * sum;
    }

    // Tone LPF Filter Coeff (1.5kHz to 18kHz)
    float lpfCutoff = 1500.0f + handle->smoothedTone * 16500.0f;
    float lpfCoeff = 1.0f - (float)exp(-2.0 * 3.14159265358979323846 * (double)lpfCutoff / (double)handle->sampleRate);

    // Filter and write back
    for (int i = 0; i < 4; ++i) {
        handle->lpfState[i] += lpfCoeff * (diff[i] - handle->lpfState[i]);
        float damped = handle->lpfState[i];

        bufs[i][handle->writeIdx[i]] = inSample + damped * handle->smoothedDwell;
        handle->writeIdx[i] = (handle->writeIdx[i] + 1) % MOOER_REVERB_MAX_DELAY;
    }

    float wet = (fdnOut[0] + fdnOut[2]) * 0.5f;
    return inSample * (1.0f - handle->smoothedMix) + wet * handle->smoothedMix;
}

void MooerIRReverb_ProcessBuffer(MooerIRReverb* handle, float* buffer, int numSamples) {
    if (!handle || !buffer) return;
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = MooerIRReverb_ProcessSample(handle, buffer[i]);
    }
}
