#include "mooer_harmonic_tremolo.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float compute_lfo(double phase, MooerLFOWaveform wave) {
    double p = phase - floor(phase);
    switch (wave) {
        case MOOER_LFO_SINE:
            return (float)sin(2.0 * M_PI * p);
        case MOOER_LFO_TRIANGLE:
            return (float)(4.0 * fabs(p - 0.5) - 1.0);
        case MOOER_LFO_TUBE_SINE: {
            double raw = sin(2.0 * M_PI * p);
            return (float)(tanh(1.8 * raw) / 0.9468); // 0.9468 = tanh(1.8)
        }
        case MOOER_LFO_SQUARE:
            return (p < 0.5) ? 0.95f : -0.95f;
        default:
            return (float)sin(2.0 * M_PI * p);
    }
}

static float apply_warmth(float x, float warmth) {
    if (warmth <= 0.001f) return x;
    float drive = 1.0f + warmth * 2.0f;
    float in = x * drive;
    float sat = in - 0.15f * (in * in) - 0.05f * (in * in * in);
    float lim = (float)tanh(sat);
    return x * (1.0f - warmth) + (lim / drive) * warmth;
}

void MooerHarmonicTremolo_Init(MooerHarmonicTremolo* handle, float sampleRate) {
    if (!handle) return;
    handle->sampleRate = (sampleRate > 0.0f) ? sampleRate : 44100.0f;
    handle->rateHz = 3.5f;
    handle->depth = 0.85f;
    handle->crossoverHz = 650.0f;
    handle->resonanceQ = 0.707f;
    handle->warmth = 0.35f;
    handle->mix = 1.0f;
    handle->waveform = MOOER_LFO_TUBE_SINE;
    MooerHarmonicTremolo_Reset(handle);
}

void MooerHarmonicTremolo_Reset(MooerHarmonicTremolo* handle) {
    if (!handle) return;
    handle->lfoPhase = 0.0;
    handle->svf_ic1eq = 0.0;
    handle->svf_ic2eq = 0.0;
    handle->smoothedRate = handle->rateHz;
    handle->smoothedDepth = handle->depth;
    handle->smoothedCrossover = handle->crossoverHz;
}

float MooerHarmonicTremolo_ProcessSample(MooerHarmonicTremolo* handle, float inSample) {
    if (!handle) return inSample;

    // Smooth parameters
    handle->smoothedRate += 0.005f * (handle->rateHz - handle->smoothedRate);
    handle->smoothedDepth += 0.005f * (handle->depth - handle->smoothedDepth);
    handle->smoothedCrossover += 0.005f * (handle->crossoverHz - handle->smoothedCrossover);

    // LFO phase advance
    double phaseInc = (double)handle->smoothedRate / (double)handle->sampleRate;
    handle->lfoPhase += phaseInc;
    if (handle->lfoPhase >= 1.0) handle->lfoPhase -= 1.0;

    float lfoLow = compute_lfo(handle->lfoPhase, handle->waveform);
    float lfoHigh = compute_lfo(handle->lfoPhase + 0.5, handle->waveform); // 180 deg out of phase

    // SVF Crossover Filter
    float g = (float)tan(M_PI * (double)handle->smoothedCrossover / (double)handle->sampleRate);
    float k = 1.0f / handle->resonanceQ;
    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;

    float v3 = inSample - (float)handle->svf_ic2eq;
    float v1 = a1 * (float)handle->svf_ic1eq + a2 * v3;
    float v2 = (float)handle->svf_ic2eq + a2 * (float)handle->svf_ic1eq + a3 * v3;

    handle->svf_ic1eq = 2.0 * v1 - handle->svf_ic1eq;
    handle->svf_ic2eq = 2.0 * v2 - handle->svf_ic2eq;

    float lowBand = v2;
    float highBand = inSample - k * v1 - v2;

    // Saturation
    lowBand = apply_warmth(lowBand, handle->warmth);
    highBand = apply_warmth(highBand, handle->warmth);

    // Modulation (180 out-of-phase LFO)
    float modLowGain = 1.0f - handle->smoothedDepth * 0.5f * (1.0f + lfoLow);
    float modHighGain = 1.0f - handle->smoothedDepth * 0.5f * (1.0f + lfoHigh);

    float wet = (lowBand * modLowGain) + (highBand * modHighGain);
    return inSample * (1.0f - handle->mix) + wet * handle->mix;
}

void MooerHarmonicTremolo_ProcessBuffer(MooerHarmonicTremolo* handle, float* buffer, int numSamples) {
    if (!handle || !buffer) return;
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = MooerHarmonicTremolo_ProcessSample(handle, buffer[i]);
    }
}
