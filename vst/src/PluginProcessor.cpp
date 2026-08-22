#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EffectConfig.h"

#include <algorithm>
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

    // Footswitch press/hold as bool parameters -> LV2 control ports a MOD
    // pedalboard (or DAW automation) can map to a momentary footswitch.
    footswitchPress_ = new juce::AudioParameterBool(
        juce::ParameterID("footswitchPress", 1), "Footswitch Press", false);
    footswitchHold_ = new juce::AudioParameterBool(
        juce::ParameterID("footswitchHold", 1), "Footswitch Hold", false);
    addParameter(footswitchPress_);
    addParameter(footswitchHold_);
}

const juce::String FxPatchAudioProcessor::getName() const
{
    return juce::String("FxPatch_") + FX_EFFECT_NAME;
}

void FxPatchAudioProcessor::prepareToPlay(double sampleRate, int maxBlockSize)
{
    const bool nominalRate = std::abs(sampleRate - (double) Patch::kSampleRate) < 1.0;
    sampleRateOk_.store(nominalRate, std::memory_order_relaxed);

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

    // Resampling setup for any host rate other than 48 kHz. JUCE treats
    // prepareToPlay as a stream restart, so the (stateful)
    // juce::LagrangeInterpolator instances always reset here regardless of
    // whether resampling is active this call -- cheap, and correct if the
    // host rate changes between prepareToPlay calls.
    downL_.reset();
    downR_.reset();
    upL_.reset();
    upR_.reset();

    if (!nominalRate && sampleRate > 0.0)
    {
        // speedRatio semantics (juce_GenericInterpolator.h's process()):
        // "the number of input samples to use for each output sample."
        downRatio_ = sampleRate / (double) Patch::kSampleRate;
        upRatio_ = (double) Patch::kSampleRate / sampleRate;

        // Upper bound on the 48 kHz scratch length a maxBlockSize host-rate
        // block can produce, plus a small margin: process()'s own "at least
        // speedRatio * numOutputSamplesToProduce input samples" contract
        // can round up to needing a couple of extra scratch samples at a
        // block boundary (see processBlock). Grows only, never shrinks --
        // matches workingBuffer_'s allocation-guard pattern above.
        const int maxPatchSamples =
            (int) std::ceil((double) maxBlockSize * (double) Patch::kSampleRate / sampleRate) + 8;
        if ((int) scratchLeft_.size() < maxPatchSamples)
        {
            scratchLeft_.assign((size_t) maxPatchSamples, 0.0f);
            scratchRight_.assign((size_t) maxPatchSamples, 0.0f);
        }
    }
}

bool FxPatchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Stereo in / stereo out only. Mono is deliberately rejected: with a single
    // shared buffer the corpus pattern `left[i] = ...; right[i] = ...;` would
    // feed the already-processed left sample into the right channel's state and
    // overwrite the left output. Stereo-only matches the pedal's contract and
    // sidesteps the aliasing entirely; hosts on mono tracks will adapt.
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo()) return false;
    return mainIn == mainOut;
}

void FxPatchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Drain footswitch actions queued by the editor's momentary buttons.
    const int press = pressRequests_.load(std::memory_order_relaxed);
    for (; pressSeen_ < press; ++pressSeen_)
        patch_->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchPress));
    const int hold = holdRequests_.load(std::memory_order_relaxed);
    for (; holdSeen_ < hold; ++holdSeen_)
        patch_->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));

    // Footswitch bool parameters (LV2 control ports / DAW automation): fire on
    // the rising edge so one momentary press maps to exactly one action.
    const bool pressParam = footswitchPress_->get();
    if (pressParam && ! pressParamPrev_)
        patch_->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchPress));
    pressParamPrev_ = pressParam;
    const bool holdParam = footswitchHold_->get();
    if (holdParam && ! holdParamPrev_)
        patch_->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
    holdParamPrev_ = holdParam;

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

    // Patches assume exactly 48 kHz. Stereo layout is enforced by
    // isBusesLayoutSupported, so two distinct channel buffers are guaranteed
    // in both branches below.
    if (numChannels >= 2 && numSamples > 0)
    {
        float* left  = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);

        if (sampleRateOk_.load(std::memory_order_relaxed))
        {
            patch_->processAudio(std::span<float>(left,  (size_t) numSamples),
                                 std::span<float>(right, (size_t) numSamples));
        }
        else
        {
            // Resample host-rate -> 48 kHz -> processAudio -> host-rate.
            // numPatchSamples is floor'd so downL_/downR_ never need more
            // input than this block actually has (process()'s contract:
            // "must contain at least speedRatio * numOutputSamplesToProduce
            // samples", no bounds checking of its own).
            const int numPatchSamples = (int) std::floor((double) numSamples / downRatio_);

            if (numPatchSamples > 0 && numPatchSamples <= (int) scratchLeft_.size())
            {
                // Read the full host-rate block into scratch before anything
                // overwrites left/right in place below.
                downL_.process(downRatio_, left, scratchLeft_.data(), numPatchSamples);
                downR_.process(downRatio_, right, scratchRight_.data(), numPatchSamples);

                // The upsample stage's own "at least upRatio_ * numSamples"
                // requirement is the exact reciprocal of how numPatchSamples
                // was floor'd above, so it can read a sample or two past
                // numPatchSamples. Silence that tail rather than leaving
                // whatever processed audio an earlier, possibly
                // differently-sized block left there -- a safer failure mode
                // than replaying stale audio, at the cost of an occasional
                // near-silent blip. If this is audible in practice, the
                // documented fallback (see docs/vst-host-plan.md) is a
                // persistent-FIFO resampler that carries an unconsumed tail
                // between blocks instead of resampling each block in
                // isolation.
                std::fill(scratchLeft_.begin() + numPatchSamples, scratchLeft_.end(), 0.0f);
                std::fill(scratchRight_.begin() + numPatchSamples, scratchRight_.end(), 0.0f);

                patch_->processAudio(std::span<float>(scratchLeft_.data(), (size_t) numPatchSamples),
                                     std::span<float>(scratchRight_.data(), (size_t) numPatchSamples));

                upL_.process(upRatio_, scratchLeft_.data(), left, numSamples);
                upR_.process(upRatio_, scratchRight_.data(), right, numSamples);
            }
            // else: numPatchSamples computed as 0 or exceeded scratch
            // capacity from prepareToPlay's sizing -- shouldn't happen given
            // that sizing, but leaving the block untouched (silence stays
            // whatever JUCE pre-filled the buffer with) beats reading past
            // the scratch buffer.
        }
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
