// source/dsp/lfo.h — phase-accumulator LFOs (sine and triangle).
//
// Harvested from effects/harmonica.cpp:243-249 (sine-LFO phase logic, the
// same idiom as chorus.cpp) and effects/phase_90.cpp:54-58 (triangle LFO
// shape).
//
// Both LFOs hold normalized phase in [0, 1) and increment by
// `rateHz / sampleRate` per sample. Wrap with a subtract, not `fmod`, to
// stay branch-light on Cortex-M7.
//
// Both classes also expose a stateless `value(phase)` static for callers
// that already manage phase externally (the phase_90 idiom). Choose the
// API that matches your call site: stateful `tick()` for new effects;
// stateless `value()` for code that already keeps phase as a member.
//
// CPU cost: SineLfo costs one sinf per sample. TriangleLfo is branch-free:
// an `fabsf`, a subtract, and a multiply-add.

#pragma once

#include <cmath>

#include "../Patch.h"

namespace dsp
{

class SineLfo
{
public:
    // Stateless: sine wave value for a normalized phase in [0, 1).
    static float value(float phase)
    {
        constexpr float kTwoPi = 6.283185307f;
        return sinf(phase * kTwoPi);
    }

    void setRateHz(float rateHz)
    {
        inc_ = rateHz / static_cast<float>(Patch::kSampleRate);
    }

    void reset(float phase = 0.0f) { phase_ = phase; }

    // Advance one sample and return the new sine value.
    float tick()
    {
        const float y = value(phase_);
        phase_ += inc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        return y;
    }

    float phase() const { return phase_; }

private:
    float phase_ = 0.0f;
    float inc_   = 0.0f;
};

class TriangleLfo
{
public:
    // Stateless: triangle wave value (range [-1, +1]) for normalized phase.
    static float value(float phase)
    {
        const float wrapped = phase - floorf(phase);
        return 1.0f - 4.0f * fabsf(wrapped - 0.5f);
    }

    void setRateHz(float rateHz)
    {
        inc_ = rateHz / static_cast<float>(Patch::kSampleRate);
    }

    void reset(float phase = 0.0f) { phase_ = phase; }

    float tick()
    {
        const float y = value(phase_);
        phase_ += inc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
        return y;
    }

    float phase() const { return phase_; }

private:
    float phase_ = 0.0f;
    float inc_   = 0.0f;
};

}  // namespace dsp
