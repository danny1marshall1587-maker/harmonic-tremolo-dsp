#ifndef PLUGIN_EDITOR_REVERB_H
#define PLUGIN_EDITOR_REVERB_H

#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_extra/juce_gui_extra.h>
 #include <juce_audio_processors/juce_audio_processors.h>
#endif

#include "PluginProcessor.h"

class CyberWaveReverbAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              public juce::Timer,
                                              public juce::FileDragAndDropTarget
{
public:
    CyberWaveReverbAudioProcessorEditor (CyberWaveReverbAudioProcessor&);
    ~CyberWaveReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // FileDragAndDropTarget interface
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    CyberWaveReverbAudioProcessor& audioProcessor;

    // Custom Blue Neon Look & Feel
    class BlueCyberLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        BlueCyberLookAndFeel() {
            setColour(juce::Slider::thumbColourId, juce::Colour(0xff00f2fe));
            setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff38bdf8));
            setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1e293b));
            setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
            setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff8fafc));
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider& slider) override {
            juce::ignoreUnused(slider);
            auto radius = (float) juce::jmin(width / 2, height / 2) - 6.0f;
            auto centreX = (float) x + (float) width * 0.5f;
            auto centreY = (float) y + (float) height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            // Dark outer ring
            g.setColour(juce::Colour(0xff0f172a));
            g.fillEllipse(rx, ry, rw, rw);
            g.setColour(juce::Colour(0xff334155));
            g.drawEllipse(rx, ry, rw, rw, 1.5f);

            // Cyan Active Arc
            juce::Path arc;
            arc.addCentredArc(centreX, centreY, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(juce::Colour(0xff00f2fe));
            g.strokePath(arc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Glowing Indicator Pointer
            juce::Path p;
            auto pointerLength = radius * 0.65f;
            auto pointerThickness = 3.0f;
            p.addRectangle(-pointerThickness * 0.5f, -radius + 4.0f, pointerThickness, pointerLength);
            p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

            g.setColour(juce::Colour(0xff38bdf8));
            g.fillPath(p);
        }
    };

    BlueCyberLookAndFeel customLookAndFeel;

    // Controls
    juce::Slider dwellSlider, toneSlider, mixSlider;
    juce::Label dwellLabel, toneLabel, mixLabel;

    juce::Slider preDelaySlider, erLevelSlider, hpfSlider, lpfSlider;
    juce::Label preDelayLabel, erLevelLabel, hpfLabel, lpfLabel;

    juce::Slider duckingAmountSlider, duckingReleaseSlider;
    juce::Label duckingAmountLabel, duckingReleaseLabel;

    juce::ToggleButton gateToggle;
    juce::Slider gateThresholdSlider, gateHoldSlider, gateReleaseSlider;
    juce::Label gateThresholdLabel, gateHoldLabel, gateReleaseLabel;

    juce::TextButton advancedToggleButton;
    juce::TextButton exportProfileButton;

    // Presets
    juce::ComboBox presetBox;
    juce::Label presetLabel;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> dwellAttach, toneAttach, mixAttach;
    std::unique_ptr<SliderAttachment> preDelayAttach, erLevelAttach, hpfAttach, lpfAttach;
    std::unique_ptr<SliderAttachment> duckingAmountAttach, duckingReleaseAttach;
    std::unique_ptr<ButtonAttachment> gateToggleAttach;
    std::unique_ptr<SliderAttachment> gateThresholdAttach, gateHoldAttach, gateReleaseAttach;
    std::unique_ptr<ButtonAttachment> advancedToggleAttach;

    std::unique_ptr<juce::FileChooser> fileChooser;

    bool isAdvancedMode = false;
    float animPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyberWaveReverbAudioProcessorEditor)
};

#endif // PLUGIN_EDITOR_REVERB_H
