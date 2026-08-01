#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "PluginProcessor.h"

// --- Modern Custom LookAndFeel for Sleek Glowing Knobs ---
class ModernRotaryLookAndFeel : public juce::LookAndFeel_V4 {
public:
    ModernRotaryLookAndFeel() {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00f2fe));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1e293b));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override {
        auto radius = (float)juce::jmin(width, height) / 2.0f - 8.0f;
        auto centreX = (float)x + (float)width  * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Background Arc
        juce::Path bgPath;
        bgPath.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff1e293b));
        g.strokePath(bgPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Arc (Neon Gradient)
        if (slider.isEnabled()) {
            juce::Path activePath;
            activePath.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            juce::ColourGradient grad(juce::Colour(0xff00f2fe), centreX - radius, centreY,
                                       juce::Colour(0xff4facfe), centreX + radius, centreY, false);
            g.setGradientFill(grad);
            g.strokePath(activePath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Inner Dial Body
        g.setColour(juce::Colour(0xff0f172a));
        g.fillEllipse(rx + 6.0f, ry + 6.0f, rw - 12.0f, rw - 12.0f);

        // Dial Outline
        g.setColour(juce::Colour(0xff334155));
        g.drawEllipse(rx + 6.0f, ry + 6.0f, rw - 12.0f, rw - 12.0f, 1.5f);

        // Pointer Needle
        juce::Path p;
        auto pointerLength = radius * 0.55f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius + 8.0f, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        g.setColour(juce::Colour(0xffffffff));
        g.fillPath(p);
    }
};

class HarmonicTremoloAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    HarmonicTremoloAudioProcessorEditor (HarmonicTremoloAudioProcessor&);
    ~HarmonicTremoloAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HarmonicTremoloAudioProcessor& audioProcessor;
    ModernRotaryLookAndFeel customLookAndFeel;

    // Controls
    juce::Slider rateSlider, depthSlider, crossoverSlider, warmthSlider, qSlider, stereoPhaseSlider, mixSlider;
    juce::ComboBox waveformBox;

    juce::Label rateLabel, depthLabel, crossoverLabel, warmthLabel, qLabel, stereoPhaseLabel, mixLabel, waveformLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> rateAttach, depthAttach, crossoverAttach, warmthAttach, qAttach, stereoPhaseAttach, mixAttach;
    std::unique_ptr<ComboBoxAttachment> waveformAttach;

    // Animation state
    float currentLFOPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicTremoloAudioProcessorEditor)
};
