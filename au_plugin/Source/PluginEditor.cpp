#include "PluginProcessor.h"
#include "PluginEditor.h"

HarmonicTremoloAudioProcessorEditor::HarmonicTremoloAudioProcessorEditor (HarmonicTremoloAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xfffb923c)); // Vibrant Warm Orange Title
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        addAndMakeVisible(slider);
    };

    setupSlider(rateSlider, rateLabel, "RATE (HZ)");
    setupSlider(depthSlider, depthLabel, "DEPTH");
    setupSlider(crossoverSlider, crossoverLabel, "CROSSOVER");
    setupSlider(warmthSlider, warmthLabel, "TUBE WARMTH");
    setupSlider(qSlider, qLabel, "RESONANCE Q");
    setupSlider(stereoPhaseSlider, stereoPhaseLabel, "STEREO PHASE");
    setupSlider(mixSlider, mixLabel, "DRY/WET MIX");

    waveformBox.addItemList(juce::StringArray{"Sine", "Triangle", "Tube Sine", "Square"}, 1);
    waveformBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1e293b));
    waveformBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8fafc));
    waveformBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffff7700));
    addAndMakeVisible(waveformBox);

    waveformLabel.setText("LFO WAVEFORM", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centred);
    waveformLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    waveformLabel.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8)); // Electric Blue Accent
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

    setSize (780, 420);
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
    // Background Dark Silk Base (macOS Dark Palette)
    g.fillAll (juce::Colour(0xff090d16));

    auto bounds = getLocalBounds().toFloat();
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();

    // Fetch current rate and depth for animation pulse intensity
    float depth = static_cast<float>(depthSlider.getValue());
    float lfoValLow = std::sin(2.0f * juce::MathConstants<float>::pi * currentLFOPhase);
    float lfoValHigh = -lfoValLow; // 180 deg out of phase

    // --- Dynamic Animated Dual Crossover Glows ---
    float pulseRadius1 = 130.0f + 80.0f * (0.5f + 0.5f * lfoValLow) * depth;
    float pulseRadius2 = 130.0f + 80.0f * (0.5f + 0.5f * lfoValHigh) * depth;

    // Low-band Warm Orange Radial Glow
    juce::ColourGradient lowGrad(juce::Colour(0x40ff7700), centreX - 160.0f, centreY,
                                 juce::Colour(0x00000000), centreX - 160.0f + pulseRadius1, centreY, true);
    g.setGradientFill(lowGrad);
    g.fillEllipse(centreX - 160.0f - pulseRadius1, centreY - pulseRadius1, pulseRadius1 * 2.0f, pulseRadius1 * 2.0f);

    // High-band Electric Cyan/Blue Radial Glow (Anti-phase)
    juce::ColourGradient highGrad(juce::Colour(0x4000f2fe), centreX + 160.0f, centreY,
                                   juce::Colour(0x00000000), centreX + 160.0f + pulseRadius2, centreY, true);
    g.setGradientFill(highGrad);
    g.fillEllipse(centreX + 160.0f - pulseRadius2, centreY - pulseRadius2, pulseRadius2 * 2.0f, pulseRadius2 * 2.0f);

    // --- Glassmorphic Header Card ---
    g.setColour(juce::Colour(0x1a1e293b));
    g.fillRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 65.0f, 12.0f);
    g.setColour(juce::Colour(0x33334155));
    g.drawRoundedRectangle(20.0f, 15.0f, bounds.getWidth() - 40.0f, 65.0f, 12.0f, 1.5f);

    // Title & Subtitle with Vibrant Orange & Blue Contrast
    g.setColour(juce::Colour(0xfff8fafc));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("HARMONIC WOBBLE TREM", 40, 24, 400, 28, juce::Justification::left);

    g.setColour(juce::Colour(0xfffb923c)); // Warm Orange Accent Subtitle
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("OpenDSP Dual Anti-Phase Crossover Engine", 40, 50, 400, 20, juce::Justification::left);

    // Dynamic Dual Anti-Phase Waveform Visualizer Ribbon
    juce::Path wavePathLow;
    juce::Path wavePathHigh;
    float waveY = 48.0f;
    float startX = bounds.getWidth() - 280.0f;

    wavePathLow.startNewSubPath(startX, waveY);
    wavePathHigh.startNewSubPath(startX, waveY);

    for (float x = 0; x < 240.0f; x += 4.0f) {
        float p = currentLFOPhase + (x / 240.0f);
        float yL = waveY + 15.0f * std::sin(2.0f * juce::MathConstants<float>::pi * p) * depth;
        float yH = waveY - 15.0f * std::sin(2.0f * juce::MathConstants<float>::pi * p) * depth;
        wavePathLow.lineTo(startX + x, yL);
        wavePathHigh.lineTo(startX + x, yH);
    }

    // Draw Low Band (Orange) & High Band (Blue) LFO ribbons
    g.setColour(juce::Colour(0xffff7700));
    g.strokePath(wavePathLow, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xff00f2fe));
    g.strokePath(wavePathHigh, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void HarmonicTremoloAudioProcessorEditor::resized()
{
    int knobW = 100;
    int knobH = 115;
    int startY = 100;
    int gapX = 18;
    int startX = 35;

    // Row 1: Main Controls (Label TOP -> Knob -> Value Box BOTTOM)
    auto layoutControl = [knobW, knobH](juce::Label& label, juce::Slider& slider, int x, int y) {
        label.setBounds(x, y, knobW, 18);
        slider.setBounds(x, y + 20, knobW, knobH - 20);
    };

    layoutControl(rateLabel, rateSlider, startX, startY);
    layoutControl(depthLabel, depthSlider, startX + (knobW + gapX), startY);
    layoutControl(crossoverLabel, crossoverSlider, startX + 2 * (knobW + gapX), startY);
    layoutControl(warmthLabel, warmthSlider, startX + 3 * (knobW + gapX), startY);
    layoutControl(qLabel, qSlider, startX + 4 * (knobW + gapX), startY);
    layoutControl(stereoPhaseLabel, stereoPhaseSlider, startX + 5 * (knobW + gapX), startY);

    // Row 2: Waveform & Mix
    int row2Y = startY + knobH + 30;

    waveformLabel.setBounds(200, row2Y, 160, 18);
    waveformBox.setBounds(200, row2Y + 22, 160, 32);

    layoutControl(mixLabel, mixSlider, 440, row2Y - 10);
}
