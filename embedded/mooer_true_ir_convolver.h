#ifndef MOOER_TRUE_IR_CONVOLVER_H
#define MOOER_TRUE_IR_CONVOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MOOER_IR_HEAD_SIZE 128  // Zero-latency FIR head size
#define MOOER_IR_MAX_TAIL  2048 // Maximum tail samples for low-power pedals (~42ms tail)

typedef struct {
    float sampleRate;
    float mix;
    float hpfHz;
    float lpfHz;

    float headBuffer[MOOER_IR_HEAD_SIZE];
    float headIR[MOOER_IR_HEAD_SIZE];
    int headWriteIdx;

    float tailBuffer[MOOER_IR_MAX_TAIL];
    float tailIR[MOOER_IR_MAX_TAIL];
    int tailWriteIdx;
    int tailLength;

    float lpfState;
    float hpfState;
} MooerTrueIRConvolver;

void MooerTrueIRConvolver_Init(MooerTrueIRConvolver* handle, float sampleRate);
void MooerTrueIRConvolver_Reset(MooerTrueIRConvolver* handle);
bool MooerTrueIRConvolver_LoadIR(MooerTrueIRConvolver* handle, const float* irData, int numSamples);

float MooerTrueIRConvolver_ProcessSample(MooerTrueIRConvolver* handle, float inSample);
void MooerTrueIRConvolver_ProcessBuffer(MooerTrueIRConvolver* handle, float* buffer, int numSamples);

#ifdef __cplusplus
}
#endif

#endif // MOOER_TRUE_IR_CONVOLVER_H
