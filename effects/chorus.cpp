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
//   Modulation range:   ±1–10 ms peak deviation (knob-controlled)
//   Delay line length:  2400 samples per channel (~50 ms, uses ~9.4 KB each)
//   Total working buf:  4800 floats out of 2,400,000 available
//
// Fractional delay:
//   Linear interpolation between adjacent samples. Minimum read distance is
//   720 - 480 = 240 samples, so the read position is always positive and the
//   delay line never underruns.
//
// Controls:
//   Left knob  — Rate  (0.1–5 Hz, log taper — feels linear across musical tempos)
//   Mid knob   — Depth (1–10 ms modulation depth, linear)
//   Right knob — Mix   (dry/wet blend, 0=dry, 1=full wet)
//
// Footswitch (press or hold): bypass toggle
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

        // Depth: linear 1 ms–10 ms → 48–480 samples peak modulation
        const float depthSamples = (0.001f + depth_ * 0.009f) * static_cast<float>(kSampleRate);

        constexpr float kCenter    = 720.0f; // 15 ms center delay
        constexpr float kTwoPi    = 6.283185f;

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];

            // Write dry input into delay lines
            delayL_[writeL_] = dryL;
            delayR_[writeR_] = dryR;

            // Compute modulated read positions
            float readPosL = static_cast<float>(writeL_) - kCenter
                             - depthSamples * sinf(lfoPhaseL_ * kTwoPi);
            float readPosR = static_cast<float>(writeR_) - kCenter
                             - depthSamples * sinf(lfoPhaseR_ * kTwoPi);

            if (readPosL < 0.0f) readPosL += static_cast<float>(kDelayLen);
            if (readPosR < 0.0f) readPosR += static_cast<float>(kDelayLen);

            // Linear interpolation
            const float wetL = lerpDelay(delayL_, readPosL);
            const float wetR = lerpDelay(delayR_, readPosR);

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

            left[i]  = dryL * (1.0f - mix_) + wetL * mix_;
            right[i] = dryR * (1.0f - mix_) + wetR * mix_;
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
    static float lerpDelay(const float* buf, float pos)
    {
        const int   idx0 = static_cast<int>(pos) % kDelayLen;
        const int   idx1 = (idx0 + 1) % kDelayLen;
        const float frac = pos - static_cast<float>(static_cast<int>(pos));
        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }

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
