#include "PluginProcessor.h"
#include "PluginEditor.h"

CyberWaveReverbAudioProcessor::CyberWaveReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

CyberWaveReverbAudioProcessor::~CyberWaveReverbAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CyberWaveReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("dwell", "Dwell / Size", 0.1f, 0.98f, 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tone", "Tone Damping", 0.0f, 1.0f, 0.70f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Dry/Wet Mix", 0.0f, 1.0f, 0.40f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("preDelay", "Pre-Delay (ms)", 0.0f, 100.0f, 15.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("erLevel", "Early Reflections", 0.0f, 1.0f, 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hpf", "HPF Cutoff (Hz)", 20.0f, 1000.0f, 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lpf", "LPF Cutoff (Hz)", 1000.0f, 20000.0f, 12000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("duckingAmount", "Ducking Depth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("duckingRelease", "Ducking Release (ms)", 10.0f, 1000.0f, 250.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("gateEnabled", "Gate Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateThreshold", "Gate Threshold (dB)", -60.0f, 0.0f, -36.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateHold", "Gate Hold (ms)", 0.0f, 500.0f, 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gateRelease", "Gate Release (ms)", 10.0f, 1000.0f, 200.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("advancedMode", "Advanced Mode", false));

    return { params.begin(), params.end() };
}

void CyberWaveReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    mEngine.prepare(sampleRate);
}

void CyberWaveReverbAudioProcessor::releaseResources()
{
}

bool CyberWaveReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void CyberWaveReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Sync APVTS parameters to engine
    mEngine.setDwell(apvts.getRawParameterValue("dwell")->load());
    mEngine.setTone(apvts.getRawParameterValue("tone")->load());
    mEngine.setMix(apvts.getRawParameterValue("mix")->load());

    mEngine.setPreDelayMs(apvts.getRawParameterValue("preDelay")->load());
    mEngine.setErLevel(apvts.getRawParameterValue("erLevel")->load());
    mEngine.setHpfCutoffHz(apvts.getRawParameterValue("hpf")->load());
    mEngine.setLpfCutoffHz(apvts.getRawParameterValue("lpf")->load());

    mEngine.setDuckingAmount(apvts.getRawParameterValue("duckingAmount")->load());
    mEngine.setDuckingReleaseMs(apvts.getRawParameterValue("duckingRelease")->load());

    mEngine.setGateEnabled(apvts.getRawParameterValue("gateEnabled")->load() > 0.5f);
    mEngine.setGateThresholdDb(apvts.getRawParameterValue("gateThreshold")->load());
    mEngine.setGateHoldMs(apvts.getRawParameterValue("gateHold")->load());
    mEngine.setGateReleaseMs(apvts.getRawParameterValue("gateRelease")->load());

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    const float* const* inputData = buffer.getArrayOfReadPointers();
    float* const* outputData = buffer.getArrayOfWritePointers();

    mEngine.processBlock(inputData, outputData, numChannels, numSamples);
}

void CyberWaveReverbAudioProcessor::loadAndAnalyzeIRFile(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader != nullptr) {
        int numSamples = static_cast<int>(reader->lengthInSamples);
        if (numSamples > 0 && numSamples < static_cast<int>(reader->sampleRate * 12)) { // Cap at 12 seconds
            int numChans = reader->numChannels;
            juce::AudioBuffer<float> irBuf(numChans, numSamples);
            reader->read(&irBuf, 0, numSamples, 0, true, false);

            // Create mono sum buffer for full acoustic analysis
            std::vector<float> monoSum(numSamples, 0.0f);
            for (int ch = 0; ch < numChans; ++ch) {
                const float* ptr = irBuf.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    monoSum[i] += ptr[i] / static_cast<float>(numChans);
                }
            }

            mEngine.analyzeImpulseResponse(monoSum.data(), numSamples, reader->sampleRate);

            // Synchronize all APVTS parameters with the analyzed IR values
            auto updateParam = [this](const juce::String& paramId, float value) {
                if (auto* param = apvts.getParameter(paramId)) {
                    param->setValueNotifyingHost(param->convertTo0to1(value));
                }
            };

            updateParam("dwell", mEngine.getDwell());
            updateParam("tone", mEngine.getTone());
            updateParam("preDelay", mEngine.getPreDelayMs());
            updateParam("erLevel", mEngine.getErLevel());
            updateParam("hpf", mEngine.getHpfCutoffHz());
            updateParam("lpf", mEngine.getLpfCutoffHz());
        }
    }
}

juce::AudioProcessorEditor* CyberWaveReverbAudioProcessor::createEditor()
{
    return new CyberWaveReverbAudioProcessorEditor(*this);
}

void CyberWaveReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CyberWaveReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CyberWaveReverbAudioProcessor();
}
