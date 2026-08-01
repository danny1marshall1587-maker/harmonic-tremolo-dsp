#ifndef PLUGIN_PROCESSOR_REVERB_H
#define PLUGIN_PROCESSOR_REVERB_H

#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_utils/juce_audio_utils.h>
#endif

#include "IRApproxReverbEngine.hpp"

class CyberWaveReverbAudioProcessor : public juce::AudioProcessor {
public:
    CyberWaveReverbAudioProcessor();
    ~CyberWaveReverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CyberWave Reverb"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void loadAndAnalyzeIRFile(const juce::File& file);

    AudioDSP::IRApproxReverbEngine& getEngine() { return mEngine; }
    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    AudioDSP::IRApproxReverbEngine mEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyberWaveReverbAudioProcessor)
};

#endif // PLUGIN_PROCESSOR_REVERB_H
