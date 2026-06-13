// Stereo Chorus
//
// Classic modulated-delay chorus. Two independent delay lines (L/R) are each
// read at a position swept by a sine LFO. Left and right LFOs are 90° out of
// phase, which produces stereo width without any additional processing.
//
// Signal flow:
//   input → write delay line → read at (center_delay ± depth * sin(lfo_phase))
//         → blend with dry signal → output
//
// Delay parameters:
//   Center delay:       15 ms  (720 samples @ 48 kHz)
//   Modulation range:   ±1–13 ms peak deviation (knob-controlled)
//   Delay line length:  2400 samples per channel (~50 ms, uses ~9.4 KB each)
//   Total working buf:  4800 floats out of 2,400,000 available
//
// Fractional delay:
//   Linear interpolation between adjacent samples. Minimum read distance is
//   720 - 624 = 96 samples, so the read position is always positive and the
//   delay line never underruns.
//
// Controls:
//   Left knob  — Rate  (0.1–5 Hz, log taper — feels linear across musical tempos)
//   Mid knob   — Depth (1–13 ms modulation depth, linear)
//   Right knob — Mix   (dry/wet blend, 0=dry, 1=full wet; equal-power crossfade)
//
// Footswitch (press or hold): bypass toggle
//
// Expression pedal:
//   Controls Mix (same as Right knob). Heel down = 0% wet, toe down = 100% wet.
//   When the expression pedal is connected, the Right knob is ignored by the
//   firmware. Enabled via patch_agent_is_param_enabled in PatchCppWrapper.cpp.
//
// LED:
//   kLightBlueColor = active
//   kDimBlue        = bypassed
//
// To build:
//   1. Copy this file to source/PatchImpl.cpp
//   2. Change the include below to: #include "Patch.h"
//   3. make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=chorus

#include "../source/Patch.h"
#include "../source/dsp/crossfade.h"
#include "../source/dsp/fractional_delay.h"
#include "../source/dsp/lfo.h"
#include <cmath>

class PatchImpl : public Patch
{
public:
    // Delay line length per channel in samples.
    // 2400 @ 48 kHz = 50 ms — comfortably larger than center (720) + max depth (480).
    static constexpr int kDelayLen = 2400;

    void init() override
    {
        lfoPhaseL_ = 0.0f;
        lfoPhaseR_ = 0.25f; // 90° offset for stereo width
        writeL_    = 0;
        writeR_    = 0;
        rate_      = 0.5f;
        depth_     = 0.5f;
        mix_       = 0.5f;
        bypassed_  = false;
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
    {
        float* ptr = buf.data();
        delayL_ = ptr;
        delayR_ = ptr + kDelayLen;
        for (int i = 0; i < kDelayLen; ++i)
            delayL_[i] = delayR_[i] = 0.0f;
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (!delayL_)
            return;

        // Compute per-buffer constants outside the sample loop.
        // Log taper: hz = 0.1 * 50^rate  →  0.0=0.1 Hz, 0.5≈0.7 Hz, 1.0=5 Hz
        const float hz       = 0.1f * powf(50.0f, rate_);
        const float phaseInc = hz / static_cast<float>(kSampleRate);

        // Depth: linear 1 ms–13 ms → 48–624 samples peak modulation.
        // Widened from the original 1–10 ms to give a more audible "moving"
        // character at the top of the knob; 13 ms peak still leaves ≥96 samples
        // of clearance against the 720-sample center, so the read pointer never
        // crosses the write pointer.
        const float depthSamples = (0.001f + depth_ * 0.012f) * static_cast<float>(kSampleRate);

        constexpr float kCenter    = 720.0f; // 15 ms center delay

        // Equal-power crossfade on the Mix knob (was linear). Linear blends
        // produce a −3 dB dip at mix=0.5; equal-power keeps perceived loudness
        // constant across the knob so "more wet" actually reads as more chorus
        // rather than "same level, slightly filtered."
        const float mixClamped = (mix_ < 0.0f) ? 0.0f : (mix_ > 1.0f ? 1.0f : mix_);
        const auto  mixGains   = dsp::equalPower(mixClamped);
        const float dryGain    = mixGains.dry;
        const float wetGain    = mixGains.wet;

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];

            // Write dry input into delay lines
            delayL_[writeL_] = dryL;
            delayR_[writeR_] = dryR;

            // Compute modulated read positions (sine LFO via dsp::SineLfo::value).
            float readPosL = static_cast<float>(writeL_) - kCenter
                             - depthSamples * dsp::SineLfo::value(lfoPhaseL_);
            float readPosR = static_cast<float>(writeR_) - kCenter
                             - depthSamples * dsp::SineLfo::value(lfoPhaseR_);

            if (readPosL < 0.0f) readPosL += static_cast<float>(kDelayLen);
            if (readPosR < 0.0f) readPosR += static_cast<float>(kDelayLen);

            // Linear-interpolated fractional-delay read from source/dsp/.
            const float wetL = dsp::lerpRead(delayL_, kDelayLen, readPosL);
            const float wetR = dsp::lerpRead(delayR_, kDelayLen, readPosR);

            // Advance write positions
            if (++writeL_ >= kDelayLen) writeL_ = 0;
            if (++writeR_ >= kDelayLen) writeR_ = 0;

            // Advance LFO phases
            lfoPhaseL_ += phaseInc;
            if (lfoPhaseL_ >= 1.0f) lfoPhaseL_ -= 1.0f;
            lfoPhaseR_ += phaseInc;
            if (lfoPhaseR_ >= 1.0f) lfoPhaseR_ -= 1.0f;

            if (bypassed_)
                continue;

            left[i]  = dryL * dryGain + wetL * wetGain;
            right[i] = dryR * dryGain + wetR * wetGain;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (paramIdx)
        {
        case 0: return {0.0f, 1.0f, 0.5f}; // Rate
        case 1: return {0.0f, 1.0f, 0.5f}; // Depth
        case 2: return {0.0f, 1.0f, 0.5f}; // Mix
        default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        if (idx == 0) rate_  = value;
        if (idx == 1) depth_ = value;
        if (idx == 2) mix_   = value;
    }

    void handleAction(int /* idx */) override
    {
        bypassed_ = !bypassed_;
    }

    Color getStateLedColor() override
    {
        return bypassed_ ? Color::kDimBlue : Color::kLightBlueColor;
    }

private:

    float* delayL_ = nullptr;
    float* delayR_ = nullptr;
    int    writeL_ = 0;
    int    writeR_ = 0;

    float lfoPhaseL_ = 0.0f;
    float lfoPhaseR_ = 0.25f;

    float rate_     = 0.5f;
    float depth_    = 0.5f;
    float mix_      = 0.5f;
    bool  bypassed_ = false;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}
