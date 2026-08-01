#include "PluginProcessor.h"
#include "PluginEditor.h"

HarmonicTremoloAudioProcessor::HarmonicTremoloAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

HarmonicTremoloAudioProcessor::~HarmonicTremoloAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout HarmonicTremoloAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"rate", 1}, "Rate", juce::NormalisableRange<float>(0.1f, 15.0f, 0.01f, 0.5f), 3.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"depth", 1}, "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"crossover", 1}, "Crossover Freq", juce::NormalisableRange<float>(150.0f, 3500.0f, 1.0f, 0.4f), 650.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"warmth", 1}, "Tube Warmth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"q", 1}, "Resonance Q", juce::NormalisableRange<float>(0.5f, 4.0f, 0.01f), 0.707f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"stereoPhase", 1}, "Stereo Phase Offset", juce::NormalisableRange<float>(0.0f, 180.0f, 1.0f), 90.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"waveform", 1}, "LFO Waveform", juce::StringArray{"Sine", "Triangle", "Tube Sine", "Square"}, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

void HarmonicTremoloAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    dspEngine.prepare(sampleRate);
}

void HarmonicTremoloAudioProcessor::releaseResources() {}

bool HarmonicTremoloAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void HarmonicTremoloAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Sync parameters from APVTS to DSP engine
    dspEngine.setRate(apvts.getRawParameterValue("rate")->load());
    dspEngine.setDepth(apvts.getRawParameterValue("depth")->load());
    dspEngine.setCrossoverFrequency(apvts.getRawParameterValue("crossover")->load());
    dspEngine.setWarmth(apvts.getRawParameterValue("warmth")->load());
    dspEngine.setResonanceQ(apvts.getRawParameterValue("q")->load());
    dspEngine.setStereoPhaseOffset(apvts.getRawParameterValue("stereoPhase")->load());
    dspEngine.setWaveform(static_cast<AudioDSP::HarmonicTremoloEngine::LFOWaveform>(
        static_cast<int>(apvts.getRawParameterValue("waveform")->load())));
    dspEngine.setMix(apvts.getRawParameterValue("mix")->load());

    const float* const* inputData = buffer.getArrayOfReadPointers();
    float* const* outputData = buffer.getArrayOfWritePointers();

    dspEngine.processBlock(inputData, outputData, totalNumInputChannels, buffer.getNumSamples());
}

juce::AudioProcessorEditor* HarmonicTremoloAudioProcessor::createEditor() {
    return new HarmonicTremoloAudioProcessorEditor(*this);
}

void HarmonicTremoloAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void HarmonicTremoloAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new HarmonicTremoloAudioProcessor();
}
