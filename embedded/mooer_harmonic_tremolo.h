/**
 * @file mooer_harmonic_tremolo.h
 * @brief Pure C Embedded DSP Block for Mooer GE Multi-FX Series
 * 
 * High performance, zero dynamic memory allocation harmonic tremolo.
 * Compatible with ARM Cortex-M, Analog Devices SHARC, TI C6000, and generic C compilers.
 */

#ifndef MOOER_HARMONIC_TREMOLO_H
#define MOOER_HARMONIC_TREMOLO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOOER_LFO_SINE = 0,
    MOOER_LFO_TRIANGLE,
    MOOER_LFO_TUBE_SINE,
    MOOER_LFO_SQUARE
} MooerLFOWaveform;

typedef struct {
    // Parameters
    float rateHz;               // 0.1Hz - 20.0Hz
    float depth;                // 0.0 - 1.0
    float crossoverHz;          // 150.0Hz - 4000.0Hz
    float resonanceQ;           // 0.5 - 5.0
    float warmth;               // 0.0 - 1.0 (Triode saturation)
    float mix;                  // 0.0 - 1.0
    MooerLFOWaveform waveform;

    // Internal State
    float sampleRate;
    double lfoPhase;
    double svf_ic1eq;
    double svf_ic2eq;
    
    // Smoothed values
    float smoothedRate;
    float smoothedDepth;
    float smoothedCrossover;
} MooerHarmonicTremolo;

/**
 * @brief Initialize Mooer DSP Block
 */
void MooerHarmonicTremolo_Init(MooerHarmonicTremolo* handle, float sampleRate);

/**
 * @brief Reset internal filter memory and LFO
 */
void MooerHarmonicTremolo_Reset(MooerHarmonicTremolo* handle);

/**
 * @brief Process single sample frame (Mono embedded block processing)
 */
float MooerHarmonicTremolo_ProcessSample(MooerHarmonicTremolo* handle, float inSample);

/**
 * @brief Process audio buffer array in-place
 */
void MooerHarmonicTremolo_ProcessBuffer(MooerHarmonicTremolo* handle, float* buffer, int numSamples);

#ifdef __cplusplus
}
#endif

#endif // MOOER_HARMONIC_TREMOLO_H
