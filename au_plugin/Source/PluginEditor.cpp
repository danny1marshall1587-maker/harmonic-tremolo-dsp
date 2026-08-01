#include "PluginProcessor.h"
#include "PluginEditor.h"

HarmonicTremoloAudioProcessorEditor::HarmonicTremoloAudioProcessorEditor (HarmonicTremoloAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f, juce::Font::Bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(label);
    };

    setupSlider(rateSlider, rateLabel, "RATE");
    setupSlider(depthSlider, depthLabel, "DEPTH");
    setupSlider(crossoverSlider, crossoverLabel, "CROSSOVER");
    setupSlider(warmthSlider, warmthLabel, "TUBE WARMTH");
    setupSlider(qSlider, qLabel, "RESONANCE Q");
    setupSlider(stereoPhaseSlider, stereoPhaseLabel, "STEREO PHASE");
    setupSlider(mixSlider, mixLabel, "MIX");

    waveformBox.addItemList(juce::StringArray{"Sine", "Triangle", "Tube Sine", "Square"}, 1);
    addAndMakeVisible(waveformBox);

    waveformLabel.setText("LFO WAVEFORM", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centred);
    waveformLabel.setFont(juce::Font(12.0f, juce::Font::Bold));
    waveformLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(waveformLabel);

    // Attachments
    rateAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rate", rateSlider);
    depthAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "depth", depthSlider);
    crossoverAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "crossover", crossoverSlider);
    warmthAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "warmth", warmthSlider);
    qAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "q", qSlider);
    stereoPhaseAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "stereoPhase", stereoPhaseSlider);
    mixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);
    waveformAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "waveform", waveformBox);

    setSize (760, 440);
    startTimerHz(60); // 60 FPS animation timer
}

HarmonicTremoloAudioProcessorEditor::~HarmonicTremoloAudioProcessorEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void HarmonicTremoloAudioProcessorEditor::timerCallback()
{
    currentLFOPhase = audioProcessor.getEngine().getCurrentLFOPhaseL();
    repaint();
}

void HarmonicTremoloAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background Dark Canvas
    g.fillAll (juce::Colour(0xff090d16));

    auto bounds = getLocalBounds().toFloat();
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();

    // Fetch current rate and depth for animation pulse intensity
    float depth = depthSlider.getValue();
    float lfoValLow = std::sin(2.0f * juce::MathConstants<float>::pi * currentLFOPhase);
    float lfoValHigh = -lfoValLow; // 180 deg out of phase

    // --- Dynamic Animated Background Pulsing with Tremolo LFO Tempo ---
    float pulseRadius1 = 120.0f + 90.0f * (0.5f + 0.5f * lfoValLow) * depth;
    float pulseRadius2 = 120.0f + 90.0f * (0.5f + 0.5f * lfoValHigh) * depth;

    // Low-band Cyan Radial Glow
    juce::ColourGradient lowGrad(juce::Colour(0x4000f2fe), centreX - 140.0f, centreY,
                                 juce::Colour(0x00000000), centreX - 140.0f + pulseRadius1, centreY, true);
    g.setGradientFill(lowGrad);
    g.fillEllipse(centreX - 140.0f - pulseRadius1, centreY - pulseRadius1, pulseRadius1 * 2.0f, pulseRadius1 * 2.0f);

    // High-band Purple Radial Glow (Anti-phase)
    juce::ColourGradient highGrad(juce::Colour(0x409b51e0), centreX + 140.0f, centreY,
                                  juce::Colour(0x00000000), centreX + 140.0f + pulseRadius2, centreY, true);
    g.setGradientFill(highGrad);
    g.fillEllipse(centreX + 140.0f - pulseRadius2, centreY - pulseRadius2, pulseRadius2 * 2.0f, pulseRadius2 * 2.0f);

    // --- Glassmorphic Center Card Header ---
    g.setColour(juce::Colour(0x1a1e293b));
    g.fillRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 65.0f, 12.0f);
    g.setColour(juce::Colour(0x33334155));
    g.drawRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 65.0f, 12.0f, 1.5f);

    // Title & Subtitle
    g.setColour(juce::Colour(0xfff8fafc));
    g.setFont(juce::Font(22.0f, juce::Font::Bold));
    g.drawText("TRI-VERB HARMONIC TREMOLO", 40, 24, 400, 28, juce::Justification::left);

    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::Font(12.0f, juce::Font::Plain));
    g.drawText("OpenDSP Dual Anti-Phase Crossover Engine", 40, 50, 400, 20, juce::Justification::left);

    // Dynamic Tempo Visualizer Ribbon
    juce::Path wavePath;
    float waveY = 50.0f;
    float startX = bounds.getWidth() - 260.0f;
    wavePath.startNewSubPath(startX, waveY);
    for (float x = 0; x < 200.0f; x += 4.0f) {
        float p = currentLFOPhase + (x / 200.0f);
        float y = waveY + 14.0f * std::sin(2.0f * juce::MathConstants<float>::pi * p) * depth;
        wavePath.lineTo(startX + x, y);
    }
    g.setColour(juce::Colour(0xff00f2fe));
    g.strokePath(wavePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void HarmonicTremoloAudioProcessorEditor::resized()
{
    int knobW = 90;
    int knobH = 110;
    int startY = 110;
    int gapX = 18;

    // Row 1: Main Controls
    rateSlider.setBounds(40, startY, knobW, knobH);
    rateLabel.setBounds(40, startY + knobH - 22, knobW, 18);

    depthSlider.setBounds(40 + (knobW + gapX), startY, knobW, knobH);
    depthLabel.setBounds(40 + (knobW + gapX), startY + knobH - 22, knobW, 18);

    crossoverSlider.setBounds(40 + 2 * (knobW + gapX), startY, knobW, knobH);
    crossoverLabel.setBounds(40 + 2 * (knobW + gapX), startY + knobH - 22, knobW, 18);

    warmthSlider.setBounds(40 + 3 * (knobW + gapX), startY, knobW, knobH);
    warmthLabel.setBounds(40 + 3 * (knobW + gapX), startY + knobH - 22, knobW, 18);

    qSlider.setBounds(40 + 4 * (knobW + gapX), startY, knobW, knobH);
    qLabel.setBounds(40 + 4 * (knobW + gapX), startY + knobH - 22, knobW, 18);

    stereoPhaseSlider.setBounds(40 + 5 * (knobW + gapX), startY, knobW, knobH);
    stereoPhaseLabel.setBounds(40 + 5 * (knobW + gapX), startY + knobH - 22, knobW, 18);

    // Row 2: Waveform & Mix
    int row2Y = startY + knobH + 30;

    waveformBox.setBounds(200, row2Y + 20, 160, 32);
    waveformLabel.setBounds(200, row2Y, 160, 18);

    mixSlider.setBounds(420, row2Y - 10, knobW, knobH);
    mixLabel.setBounds(420, row2Y + knobH - 32, knobW, 18);
}
