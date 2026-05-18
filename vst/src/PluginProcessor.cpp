#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EffectConfig.h"

#include <cmath>
#include <span>

namespace
{
const char* const kKnobNames[endless::kParams] = {"Left", "Mid", "Right"};
}

FxPatchAudioProcessor::FxPatchAudioProcessor()
    : juce::AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    patch_ = Patch::getInstance();

    for (int i = 0; i < endless::kParams; ++i)
    {
        const auto md = patch_->getParameterMetadata(i);
        auto* p = new juce::AudioParameterFloat(
            juce::ParameterID(juce::String("knob") + juce::String(i), 1),
            kKnobNames[i],
            juce::NormalisableRange<float>(md.minValue, md.maxValue),
            md.defaultValue);
        knobParams_[(size_t) i] = p;
        lastKnob_[(size_t) i] = md.defaultValue;
        addParameter(p);
    }
}

const juce::String FxPatchAudioProcessor::getName() const
{
    return juce::String("FxPatch ") + FX_EFFECT_NAME;
}

void FxPatchAudioProcessor::prepareToPlay(double sampleRate, int /*maxBlockSize*/)
{
    sampleRateOk_.store(std::abs(sampleRate - (double) Patch::kSampleRate) < 1.0,
                        std::memory_order_relaxed);

    // 9.6 MB working buffer — allocated here (non-realtime), never per-block.
    if (workingBuffer_.size() != (size_t) Patch::kWorkingBufferSize)
        workingBuffer_.assign((size_t) Patch::kWorkingBufferSize, 0.0f);

    patch_->setWorkingBuffer(std::span<float, Patch::kWorkingBufferSize>(
        workingBuffer_.data(), Patch::kWorkingBufferSize));
    patch_->init();

    // init() resets effect state, so re-push every parameter afterwards.
    for (int i = 0; i < endless::kParams; ++i)
    {
        const float v = knobParams_[(size_t) i]->get();
        patch_->setParamValue(i, v);
        lastKnob_[(size_t) i] = v;
    }
}

bool FxPatchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FxPatchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Drain footswitch actions queued by the editor.
    const int press = pressRequests_.load(std::memory_order_relaxed);
    for (; pressSeen_ < press; ++pressSeen_)
        patch_->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchPress));
    const int hold = holdRequests_.load(std::memory_order_relaxed);
    for (; holdSeen_ < hold; ++holdSeen_)
        patch_->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));

    // Push parameter changes (setParamValue is documented audio-thread-safe).
    for (int i = 0; i < endless::kParams; ++i)
    {
        const float v = knobParams_[(size_t) i]->get();
        if (v != lastKnob_[(size_t) i])
        {
            patch_->setParamValue(i, v);
            lastKnob_[(size_t) i] = v;
        }
    }

    // Patches assume exactly 48 kHz. At any other rate, pass audio through dry
    // rather than emit subtly wrong output; the editor surfaces a warning.
    if (sampleRateOk_.load(std::memory_order_relaxed) && numChannels >= 1 && numSamples > 0)
    {
        float* left = buffer.getWritePointer(0);
        float* right = numChannels > 1 ? buffer.getWritePointer(1) : left;
        patch_->processAudio(std::span<float>(left, (size_t) numSamples),
                             std::span<float>(right, (size_t) numSamples));
    }

    ledColor_.store(static_cast<int>(patch_->getStateLedColor()),
                    std::memory_order_relaxed);
}

juce::AudioProcessorEditor* FxPatchAudioProcessor::createEditor()
{
    return new FxPatchAudioProcessorEditor(*this);
}

void FxPatchAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    for (auto* p : knobParams_)
        stream.writeFloat(p != nullptr ? p->get() : 0.0f);
}

void FxPatchAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, (size_t) sizeInBytes, false);
    for (auto* p : knobParams_)
        if (p != nullptr && ! stream.isExhausted())
            *p = stream.readFloat();
}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FxPatchAudioProcessor();
}
