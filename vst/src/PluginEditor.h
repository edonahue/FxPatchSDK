#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

// Minimal editor: three rotary knobs bound to the effect parameters, two
// momentary footswitch buttons, and a dot mirroring the patch state LED.
class FxPatchAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit FxPatchAudioProcessorEditor(FxPatchAudioProcessor&);
    ~FxPatchAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    FxPatchAudioProcessor& processor_;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    std::array<Knob, endless::kParams> knobs_;

    juce::TextButton pressButton_ {"Footswitch"};
    juce::TextButton holdButton_ {"Hold (alt voice)"};

    int ledColor_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPatchAudioProcessorEditor)
};
