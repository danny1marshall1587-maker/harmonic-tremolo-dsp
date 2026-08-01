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

    // Easy Mode 3 Main Knobs
    setupSlider(dwellSlider, dwellLabel, "DWELL / SIZE");
    setupSlider(toneSlider, toneLabel, "TONE DAMPING");
    setupSlider(mixSlider, mixLabel, "DRY/WET MIX");

    // Advanced Controls
    setupSlider(preDelaySlider, preDelayLabel, "PRE-DELAY");
    setupSlider(erLevelSlider, erLevelLabel, "EARLY REFL");
    setupSlider(hpfSlider, hpfLabel, "HPF CUTOFF");
    setupSlider(lpfSlider, lpfLabel, "LPF CUTOFF");

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

    // Export Pedal Profile (.irprof) Button
    exportProfileButton.setButtonText("EXPORT PEDAL PROFILE (.IRPROF)");
    exportProfileButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    exportProfileButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffffff));
    exportProfileButton.onClick = [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save Hardware Reverb Profile for Mooer GE / NUX MG-400 / Valeton GP-150...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("reverb_model.irprof"),
            "*.irprof");

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file != juce::File()) {
                    audioProcessor.getEngine().exportHardwareProfile(file.getFullPathName().toStdString(), file.getFileNameWithoutExtension().toStdString());
                }
            });
    };
    addAndMakeVisible(exportProfileButton);

    // Presets Box
    presetBox.addItemList(juce::StringArray{"Custom / Loaded IR", "Smooth Hall", "Bright Plate", "Cyber Space", "80s Gated Snare", "Ducked Ambient"}, 1);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e293b));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8fafc));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff00f2fe));
    presetBox.setSelectedId(1);
    presetBox.onChange = [this]() {
        int id = presetBox.getSelectedId();
        if (id == 2) { // Hall
            dwellSlider.setValue(0.85); toneSlider.setValue(0.65); mixSlider.setValue(0.45);
            hpfSlider.setValue(60.0); lpfSlider.setValue(14000.0);
        } else if (id == 3) { // Plate
            dwellSlider.setValue(0.65); toneSlider.setValue(0.90); mixSlider.setValue(0.35);
            hpfSlider.setValue(120.0); lpfSlider.setValue(18000.0);
        } else if (id == 4) { // Cyber Space
            dwellSlider.setValue(0.91); toneSlider.setValue(0.80); mixSlider.setValue(0.60);
            hpfSlider.setValue(40.0); lpfSlider.setValue(16000.0);
        } else if (id == 5) { // Gated Snare
            dwellSlider.setValue(0.70); toneSlider.setValue(0.85); mixSlider.setValue(0.50);
            gateToggle.setToggleState(true, juce::sendNotification);
            gateThresholdSlider.setValue(-30.0); gateHoldSlider.setValue(60.0); gateReleaseSlider.setValue(100.0);
        } else if (id == 6) { // Ducked Ambient
            dwellSlider.setValue(0.91); toneSlider.setValue(0.60); mixSlider.setValue(0.55);
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
    dwellAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "dwell", dwellSlider);
    toneAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tone", toneSlider);
    mixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);

    preDelayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "preDelay", preDelaySlider);
    erLevelAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "erLevel", erLevelSlider);
    hpfAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "hpf", hpfSlider);
    lpfAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "lpf", lpfSlider);

    duckingAmountAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "duckingAmount", duckingAmountSlider);
    duckingReleaseAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "duckingRelease", duckingReleaseSlider);

    gateToggleAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "gateEnabled", gateToggle);
    gateThresholdAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateThreshold", gateThresholdSlider);
    gateHoldAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateHold", gateHoldSlider);
    gateReleaseAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "gateRelease", gateReleaseSlider);

    advancedToggleAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "advancedMode", advancedToggleButton);

    setSize (780, 260);
    startTimerHz(60); // 60 FPS animation timer
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
    g.fillRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 65.0f, 12.0f);
    g.setColour(juce::Colour(0x33334155));
    g.drawRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 65.0f, 12.0f, 1.5f);

    // Title & Subtitle
    g.setColour(juce::Colour(0xfff8fafc));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("CYBERWAVE REVERB", 40, 24, 380, 28, juce::Justification::left);

    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("IR-Synthesized Algorithmic FDN Engine", 40, 50, 380, 20, juce::Justification::left);

    // Drag and Drop Zone Card
    g.setColour(juce::Colour(0x1a0284c7));
    g.fillRoundedRectangle(bounds.getWidth() - 280.0f, 22.0f, 240.0f, 50.0f, 8.0f);
    g.setColour(juce::Colour(0xff00f2fe));
    g.drawRoundedRectangle(bounds.getWidth() - 280.0f, 22.0f, 240.0f, 50.0f, 8.0f, 1.5f);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xfff8fafc));
    g.drawText("DROP IR .WAV FILE HERE", bounds.getWidth() - 280.0f, 30.0f, 240.0f, 16.0f, juce::Justification::centred);
    g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText("Auto-Fits Slim Hardware Profile", bounds.getWidth() - 280.0f, 48.0f, 240.0f, 16.0f, juce::Justification::centred);
}

void CyberWaveReverbAudioProcessorEditor::resized()
{
    int knobW = 95;
    int knobH = 115;
    int startY = 100;

    auto layoutControl = [knobW, knobH](juce::Label& label, juce::Slider& slider, int x, int y) {
        label.setBounds(x, y, knobW, 18);
        slider.setBounds(x, y + 20, knobW, knobH - 20);
    };

    // Easy Mode Row 1
    layoutControl(dwellLabel, dwellSlider, 40, startY);
    layoutControl(toneLabel, toneSlider, 150, startY);
    layoutControl(mixLabel, mixSlider, 260, startY);

    presetLabel.setBounds(380, startY, 150, 18);
    presetBox.setBounds(380, startY + 22, 150, 30);

    advancedToggleButton.setBounds(545, startY + 22, 205, 30);
    exportProfileButton.setBounds(380, startY + 62, 370, 30);

    // Advanced Tweaker Controls Visibility & Layout
    bool showAdv = isAdvancedMode;

    preDelaySlider.setVisible(showAdv); preDelayLabel.setVisible(showAdv);
    erLevelSlider.setVisible(showAdv); erLevelLabel.setVisible(showAdv);
    hpfSlider.setVisible(showAdv); hpfLabel.setVisible(showAdv);
    lpfSlider.setVisible(showAdv); lpfLabel.setVisible(showAdv);

    duckingAmountSlider.setVisible(showAdv); duckingAmountLabel.setVisible(showAdv);
    duckingReleaseSlider.setVisible(showAdv); duckingReleaseLabel.setVisible(showAdv);

    gateToggle.setVisible(showAdv);
    gateThresholdSlider.setVisible(showAdv); gateThresholdLabel.setVisible(showAdv);
    gateHoldSlider.setVisible(showAdv); gateHoldLabel.setVisible(showAdv);
    gateReleaseSlider.setVisible(showAdv); gateReleaseLabel.setVisible(showAdv);

    if (showAdv) {
        setSize(780, 600);

        int advY1 = startY + knobH + 25;

        layoutControl(preDelayLabel, preDelaySlider, 50, advY1);
        layoutControl(erLevelLabel, erLevelSlider, 160, advY1);
        layoutControl(hpfLabel, hpfSlider, 270, advY1);
        layoutControl(lpfLabel, lpfSlider, 380, advY1);

        layoutControl(duckingAmountLabel, duckingAmountSlider, 510, advY1);
        layoutControl(duckingReleaseLabel, duckingReleaseSlider, 620, advY1);

        int advY2 = advY1 + knobH + 20;

        gateToggle.setBounds(50, advY2 + 40, 130, 24);

        layoutControl(gateThresholdLabel, gateThresholdSlider, 190, advY2);
        layoutControl(gateHoldLabel, gateHoldSlider, 300, advY2);
        layoutControl(gateReleaseLabel, gateReleaseSlider, 410, advY2);
    } else {
        setSize(780, 260);
    }
}
