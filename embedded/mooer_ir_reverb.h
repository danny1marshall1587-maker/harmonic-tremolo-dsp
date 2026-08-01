#ifndef MOOER_IR_REVERB_H
#define MOOER_IR_REVERB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MOOER_REVERB_MAX_DELAY 4800 // ~100ms max FDN delay buffer size at 48kHz

typedef struct {
    char magic[8];          // "IRPROF1"
    char profileName[24];
    float dwell;
    float tone;
    float mix;
    float preDelayMs;
    float erLevel;
    float hpfHz;
    float lpfHz;
    float eqLowGainDb;
    float eqMidGainDb;
    float eqHighGainDb;
} MooerIRReverbPreset;

typedef struct {
    float sampleRate;
    float dwell;
    float tone;
    float mix;

    float preDelayMs;
    float hpfHz;
    float lpfHz;

    float eqLowGainDb;
    float eqMidGainDb;
    float eqHighGainDb;

    // Smoothed values
    float smoothedDwell;
    float smoothedTone;
    float smoothedMix;

    // FDN Delay Line Buffers (4 channels)
    float buffer0[MOOER_REVERB_MAX_DELAY];
    float buffer1[MOOER_REVERB_MAX_DELAY];
    float buffer2[MOOER_REVERB_MAX_DELAY];
    float buffer3[MOOER_REVERB_MAX_DELAY];

    int writeIdx[4];
    int delaySamples[4];

    float lpfState[4];
    float hpfState[4];

    float eqLowState;
    float eqHighState;
} MooerIRReverb;

void MooerIRReverb_Init(MooerIRReverb* handle, float sampleRate);
void MooerIRReverb_Reset(MooerIRReverb* handle);
void MooerIRReverb_LoadPreset(MooerIRReverb* handle, const MooerIRReverbPreset* preset);

float MooerIRReverb_ProcessSample(MooerIRReverb* handle, float inSample);
void MooerIRReverb_ProcessBuffer(MooerIRReverb* handle, float* buffer, int numSamples);

#ifdef __cplusplus
}
#endif

#endif // MOOER_IR_REVERB_H
