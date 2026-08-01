#ifndef PLUGIN_EDITOR_H
#define PLUGIN_EDITOR_H

#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "PluginProcessor.h"

// --- Vibrant Orange & Blue LookAndFeel for Harmonic Wobble Trem ---
class ModernOrangeBlueLookAndFeel : public juce::LookAndFeel_V4 {
public:
    ModernOrangeBlueLookAndFeel() {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff7700));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1e293b));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff8fafc));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override {
        juce::ignoreUnused(slider);
        auto radius = (float)juce::jmin(width / 2, height / 2) - 6.0f;
        auto centreX = (float)x + (float)width  * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Dark Metallic Outer Ring
        g.setColour(juce::Colour(0xff0f172a));
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(juce::Colour(0xff334155));
        g.drawEllipse(rx, ry, rw, rw, 1.5f);

        // Background Outer Track Arc
        juce::Path bgPath;
        bgPath.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff1e293b));
        g.strokePath(bgPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Arc (Vibrant Glowing Orange to Cyan Gradient)
        if (slider.isEnabled()) {
            juce::Path activePath;
            activePath.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
            juce::ColourGradient grad(juce::Colour(0xffff7700), centreX - radius, centreY,
                                       juce::Colour(0xff00f2fe), centreX + radius, centreY, false);
            g.setGradientFill(grad);
            g.strokePath(activePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Inner Dial Body (Obsidian Dark)
        g.setColour(juce::Colour(0xff090d16));
        g.fillEllipse(rx + 6.0f, ry + 6.0f, rw - 12.0f, rw - 12.0f);

        // Dial Inner Accent Ring
        g.setColour(juce::Colour(0x66ff7700));
        g.drawEllipse(rx + 6.0f, ry + 6.0f, rw - 12.0f, rw - 12.0f, 1.2f);

        // Glowing Pointer Needle
        juce::Path p;
        auto pointerLength = radius * 0.55f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius + 8.0f, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        g.setColour(juce::Colour(0xffffa726));
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
    ModernOrangeBlueLookAndFeel customLookAndFeel;

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

#endif // PLUGIN_EDITOR_H
