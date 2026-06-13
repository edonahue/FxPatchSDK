// source/dsp/parameter_smoother.h — single-pole smoother for knob inputs.
//
// Harvested from effects/tube_screamer_wdf.cpp:82-118 (same class in
// big_muff_wdf.cpp). Smooths a stepped target value (set by
// `setParamValue` or by ramp logic) into a continuous trajectory so the
// per-sample DSP that consumes the value — clip thresholds, biquad
// coefficients, delay-line read positions — does not glitch when the host
// moves a knob.
//
// Exponential approach: current = target + coeff * (current - target),
// where coeff = exp(-1 / samples). At 48 kHz, `samples = 0.001 * timeMs * 48000`,
// giving a settling time roughly 4-5x `timeMs`.
//
// CPU cost: one multiply and one subtract per sample.

#pragma once

#include <cmath>

#include "../Patch.h"

namespace dsp
{

class ParamSmoother
{
public:
    void init(float value, float timeMs)
    {
        current_ = value;
        target_  = value;
        setTimeMs(timeMs);
    }

    void setTimeMs(float timeMs)
    {
        const float samples = 0.001f * timeMs * static_cast<float>(Patch::kSampleRate);
        coeff_ = samples <= 1.0f ? 0.0f : expf(-1.0f / samples);
    }

    void setTarget(float value) { target_ = value; }

    // Set both current and target — use on bypass-off / init() so the
    // smoother does not glide from a stale value into the new one.
    void snap(float value)
    {
        current_ = value;
        target_  = value;
    }

    float process()
    {
        current_ = target_ + coeff_ * (current_ - target_);
        return current_;
    }

    // Convenience: set target and step in one call.
    float processTo(float target)
    {
        target_ = target;
        return process();
    }

    float current() const { return current_; }

private:
    float current_ = 0.0f;
    float target_  = 0.0f;
    float coeff_   = 0.0f;
};

}  // namespace dsp
