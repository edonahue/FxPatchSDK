#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Patch.h"  // resolved via the source/ include directory

// Wraps one FxPatchSDK effect (a Patch subclass) as a JUCE plugin so it can be
// auditioned in a DAW or the Standalone app before flashing to hardware.
// See ../docs/vst-host-plan.md.
class FxPatchAudioProcessor final : public juce::AudioProcessor
{
public:
    FxPatchAudioProcessor();
    ~FxPatchAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maxBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- Called by the editor (message thread) -----------------------------
    // Footswitch presses are queued lock-free and drained on the audio thread,
    // because handleAction() has no documented thread contract.
    void requestFootswitchPress() { pressRequests_.fetch_add(1, std::memory_order_relaxed); }
    void requestFootswitchHold() { holdRequests_.fetch_add(1, std::memory_order_relaxed); }
    int ledColorIndex() const { return ledColor_.load(std::memory_order_relaxed); }
    bool sampleRateOk() const { return sampleRateOk_.load(std::memory_order_relaxed); }
    juce::AudioParameterFloat* knobParameter(int idx) { return knobParams_[(size_t) idx]; }

private:
    Patch* patch_ = nullptr;
    std::vector<float> workingBuffer_;

    std::array<juce::AudioParameterFloat*, endless::kParams> knobParams_ {};
    std::array<float, endless::kParams> lastKnob_ {};

    std::atomic<int> pressRequests_ {0};
    std::atomic<int> holdRequests_ {0};
    int pressSeen_ = 0;  // audio thread only
    int holdSeen_ = 0;   // audio thread only

    // Footswitch is also exposed as bool parameters so LV2 hosts (MOD Audio
    // Desktop) and DAW automation can drive it. Fired on the rising edge: one
    // momentary press -> one action. Separate from the editor-button path
    // above because a momentary GUI click cannot safely pulse a parameter.
    juce::AudioParameterBool* footswitchPress_ = nullptr;
    juce::AudioParameterBool* footswitchHold_ = nullptr;
    bool pressParamPrev_ = false;  // audio thread only
    bool holdParamPrev_ = false;   // audio thread only

    std::atomic<int> ledColor_ {0};
    std::atomic<bool> sampleRateOk_ {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPatchAudioProcessor)
};
