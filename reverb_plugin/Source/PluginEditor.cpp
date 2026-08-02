#include "PluginProcessor.h"
#include "PluginEditor.h"

CyberWaveReverbAudioProcessorEditor::CyberWaveReverbAudioProcessorEditor (CyberWaveReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8)); // Electric Cyan Title
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        addAndMakeVisible(slider);
    };

    // Main Knobs
    setupSlider(mixSlider, mixLabel, "DRY/WET MIX");
    setupSlider(preDelaySlider, preDelayLabel, "PRE-DELAY");
    setupSlider(hpfSlider, hpfLabel, "HPF CUTOFF");
    setupSlider(lpfSlider, lpfLabel, "LPF CUTOFF");

    // Advanced Controls
    setupSlider(duckingAmountSlider, duckingAmountLabel, "DUCK DEPTH");
    setupSlider(duckingReleaseSlider, duckingReleaseLabel, "DUCK RELEASE");

    setupSlider(gateThresholdSlider, gateThresholdLabel, "GATE THRESH");
    setupSlider(gateHoldSlider, gateHoldLabel, "GATE HOLD");
    setupSlider(gateReleaseSlider, gateReleaseLabel, "GATE RELEASE");

    gateToggle.setButtonText("GATE ENABLED");
    gateToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(gateToggle);

    // Advanced Mode Toggle Button
    advancedToggleButton.setButtonText("ADVANCED TWEAKER MODE");
    advancedToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    advancedToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00f2fe));
    advancedToggleButton.setClickingTogglesState(true);
    advancedToggleButton.onClick = [this]() {
        isAdvancedMode = advancedToggleButton.getToggleState();
        resized();
    };
    addAndMakeVisible(advancedToggleButton);

    // Export Slim WAV Button for Pedals
    exportSlimWavButton.setButtonText("EXPORT SLIM IR (.WAV)");
    exportSlimWavButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    exportSlimWavButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    exportSlimWavButton.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Export Pruned Slim IR WAV File for Hardware Pedals (Mooer GE / NUX MG-400 / Valeton GP-150)...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("slim_pedal_ir.wav"),
            "*.wav");

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file != juce::File()) {
                    audioProcessor.getEngine().exportSlimWavFile(file.getFullPathName().toStdString());
                }
            });
    };
    addAndMakeVisible(exportSlimWavButton);

    // Presets Box
    presetBox.addItemList(juce::StringArray{"Custom / Loaded IR", "Clean Studio IR", "Warm Cathedral IR", "80s Gated Snare", "Ducked Ambient"}, 1);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e293b));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8fafc));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff00f2fe));
    presetBox.setSelectedId(1);
    presetBox.onChange = [this]() {
        int id = presetBox.getSelectedId();
        if (id == 2) { // Studio
            mixSlider.setValue(0.35); preDelaySlider.setValue(5.0); hpfSlider.setValue(80.0); lpfSlider.setValue(16000.0);
        } else if (id == 3) { // Cathedral
            mixSlider.setValue(0.55); preDelaySlider.setValue(35.0); hpfSlider.setValue(40.0); lpfSlider.setValue(12000.0);
        } else if (id == 4) { // Gated Snare
            mixSlider.setValue(0.50); preDelaySlider.setValue(0.0);
            gateToggle.setToggleState(true, juce::sendNotification);
            gateThresholdSlider.setValue(-30.0); gateHoldSlider.setValue(60.0); gateReleaseSlider.setValue(100.0);
        } else if (id == 5) { // Ducked Ambient
            mixSlider.setValue(0.60); preDelaySlider.setValue(20.0);
            duckingAmountSlider.setValue(0.70); duckingReleaseSlider.setValue(300.0);
        }
    };
    addAndMakeVisible(presetBox);

    presetLabel.setText("PRESET MODEL", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centred);
    presetLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    addAndMakeVisible(presetLabel);

    // Attachments
    mixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);
    preDelayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "preDelay", preDelaySlider);
    hpfAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "hpf", hpfSlider);
    lpfAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "lpf", lpfSlider);

    duckingAmountAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "duckingAmount", duckingAmountSlider);
    duckingReleaseAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "duckingRelease", duckingReleaseSlider);

    gateToggleAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "gateEnabled", gateToggle);
    gateThresholdAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateThreshold", gateThresholdSlider);
    gateHoldAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateHold", gateHoldSlider);
    gateReleaseAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateRelease", gateReleaseSlider);

    advancedToggleAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "advancedMode", advancedToggleButton);

    setSize (780, 280);
    startTimerHz(60);
}

CyberWaveReverbAudioProcessorEditor::~CyberWaveReverbAudioProcessorEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void CyberWaveReverbAudioProcessorEditor::timerCallback()
{
    animPhase += 0.05f;
    if (animPhase >= 2.0f * juce::MathConstants<float>::pi) animPhase -= 2.0f * juce::MathConstants<float>::pi;
    repaint();
}

bool CyberWaveReverbAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".aif") || file.endsWithIgnoreCase(".flac"))
            return true;
    }
    return false;
}

void CyberWaveReverbAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    for (auto file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".aif") || file.endsWithIgnoreCase(".flac")) {
            audioProcessor.loadAndAnalyzeIRFile(juce::File(file));
            presetBox.setSelectedId(1, juce::dontSendNotification);
            break;
        }
    }
}

void CyberWaveReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background Dark Canvas
    g.fillAll (juce::Colour(0xff060a12));

    auto bounds = getLocalBounds().toFloat();
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();

    // --- Dynamic Animated Blue Radial Energy Glow ---
    float pulse1 = 140.0f + 60.0f * std::sin(animPhase);
    float pulse2 = 140.0f + 60.0f * std::cos(animPhase);

    juce::ColourGradient grad1(juce::Colour(0x4000f2fe), centreX - 120.0f, centreY,
                               juce::Colour(0x00000000), centreX - 120.0f + pulse1, centreY, true);
    g.setGradientFill(grad1);
    g.fillEllipse(centreX - 120.0f - pulse1, centreY - pulse1, pulse1 * 2.0f, pulse1 * 2.0f);

    juce::ColourGradient grad2(juce::Colour(0x404f46e5), centreX + 120.0f, centreY,
                               juce::Colour(0x00000000), centreX + 120.0f + pulse2, centreY, true);
    g.setGradientFill(grad2);
    g.fillEllipse(centreX + 120.0f - pulse2, centreY - pulse2, pulse2 * 2.0f, pulse2 * 2.0f);

    // --- Glassmorphic Center Card Header ---
    g.setColour(juce::Colour(0x1a1e293b));
    g.fillRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 75.0f, 12.0f);
    g.setColour(juce::Colour(0x33334155));
    g.drawRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 75.0f, 12.0f, 1.5f);

    // Title & Subtitle
    g.setColour(juce::Colour(0xfff8fafc));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("CYBERWAVE REVERB", 40, 22, 380, 26, juce::Justification::left);

    g.setColour(juce::Colour(0xff00f2fe)); // Cyan Subtitle
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("True Partitioned Overlap-Save FFT Convolution Engine", 40, 48, 420, 20, juce::Justification::left);

    // Drag and Drop Zone Card
    g.setColour(juce::Colour(0x1a0284c7));
    g.fillRoundedRectangle(bounds.getWidth() - 280.0f, 22.0f, 240.0f, 60.0f, 8.0f);
    g.setColour(juce::Colour(0xff00f2fe));
    g.drawRoundedRectangle(bounds.getWidth() - 280.0f, 22.0f, 240.0f, 60.0f, 8.0f, 1.5f);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xfff8fafc));
    g.drawText("DROP IR .WAV FILE HERE", bounds.getWidth() - 280.0f, 28.0f, 240.0f, 16.0f, juce::Justification::centred);

    // Draw Loaded IR Waveform Minimap
    if (audioProcessor.getEngine().isLoaded()) {
        const auto& ir = audioProcessor.getEngine().getRawIrWaveform();
        int waveX = bounds.getWidth() - 270.0f;
        int waveY = 48;
        int waveW = 220;
        int waveH = 28;

        g.setColour(juce::Colour(0x6600f2fe));
        juce::Path wavePath;
        int numPts = std::min(waveW, static_cast<int>(ir.size()));
        for (int i = 0; i < numPts; ++i) {
            int idx = static_cast<int>((static_cast<double>(i) / waveW) * ir.size());
            float v = std::abs(ir[idx]);
            float px = waveX + i;
            float py = waveY + waveH - (v * waveH);
            if (i == 0) wavePath.startNewSubPath(px, py);
            else        wavePath.lineTo(px, py);
        }
        g.strokePath(wavePath, juce::PathStrokeType(1.2f));
    } else {
        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("Zero-Latency Partitioned Convolution", bounds.getWidth() - 280.0f, 50.0f, 240.0f, 16.0f, juce::Justification::centred);
    }
}

void CyberWaveReverbAudioProcessorEditor::resized()
{
    int knobW = 95;
    int knobH = 115;
    int startY = 110;

    auto layoutControl = [knobW, knobH](juce::Label& label, juce::Slider& slider, int x, int y) {
        label.setBounds(x, y, knobW, 18);
        slider.setBounds(x, y + 20, knobW, knobH - 20);
    };

    // Main Row
    layoutControl(mixLabel, mixSlider, 40, startY);
    layoutControl(preDelayLabel, preDelaySlider, 150, startY);
    layoutControl(hpfLabel, hpfSlider, 260, startY);
    layoutControl(lpfLabel, lpfSlider, 370, startY);

    presetLabel.setBounds(490, startY, 150, 18);
    presetBox.setBounds(490, startY + 22, 150, 30);

    advancedToggleButton.setBounds(490, startY + 62, 260, 30);
    exportSlimWavButton.setBounds(650, startY + 22, 100, 30);

    // Advanced Controls Layout
    bool showAdv = isAdvancedMode;

    duckingAmountSlider.setVisible(showAdv); duckingAmountLabel.setVisible(showAdv);
    duckingReleaseSlider.setVisible(showAdv); duckingReleaseLabel.setVisible(showAdv);

    gateToggle.setVisible(showAdv);
    gateThresholdSlider.setVisible(showAdv); gateThresholdLabel.setVisible(showAdv);
    gateHoldSlider.setVisible(showAdv); gateHoldLabel.setVisible(showAdv);
    gateReleaseSlider.setVisible(showAdv); gateReleaseLabel.setVisible(showAdv);

    if (showAdv) {
        setSize(780, 520);

        int advY1 = startY + knobH + 25;

        layoutControl(duckingAmountLabel, duckingAmountSlider, 50, advY1);
        layoutControl(duckingReleaseLabel, duckingReleaseSlider, 160, advY1);

        gateToggle.setBounds(280, advY1 + 40, 130, 24);

        layoutControl(gateThresholdLabel, gateThresholdSlider, 420, advY1);
        layoutControl(gateHoldLabel, gateHoldSlider, 530, advY1);
        layoutControl(gateReleaseLabel, gateReleaseSlider, 640, advY1);
    } else {
        setSize(780, 280);
    }
}
