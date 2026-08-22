// source/dsp/one_pole_filter.h — stateful one-pole low-pass / high-pass filters.
//
// Harvested from effects/tube_screamer_wdf.cpp:74-112 and
// effects/big_muff_wdf.cpp:71-109 — confirmed byte-for-byte identical file-local
// classes in both. This is the stateful companion to filter_coeff.h's
// `lpCoeff`/`hpCoeff`: those functions compute the alpha coefficient from a
// cutoff frequency; these classes hold the running filter state across
// samples and apply that coefficient. Split this way because the coefficient
// is typically recomputed once per block (the cutoff rarely changes
// per-sample) while the state must persist every sample.
//
// Recurrences:
//   Lowpass:  y[n] = y[n-1] + alpha * (x[n] - y[n-1])
//   Highpass: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
//
// Use whenever an effect needs a persistent one-pole filter stage rather
// than a one-shot coefficient computation — tone shaping, DC-adjacent
// rolloffs, or (as in big_muff_wdf/tube_screamer_wdf) body/edge splits
// feeding a nonlinear stage.
//
// CPU cost: one multiply and one or two adds per sample, per instance.

#pragma once

namespace dsp
{

class OnePoleLowpass
{
public:
    float process(float input, float alpha)
    {
        state_ += alpha * (input - state_);
        return state_;
    }

    void reset() { state_ = 0.0f; }

private:
    float state_ = 0.0f;
};

class OnePoleHighpass
{
public:
    float process(float input, float alpha)
    {
        const float output = alpha * (prevOut_ + input - prevIn_);
        prevIn_  = input;
        prevOut_ = output;
        return output;
    }

    void reset()
    {
        prevIn_  = 0.0f;
        prevOut_ = 0.0f;
    }

private:
    float prevIn_  = 0.0f;
    float prevOut_ = 0.0f;
};

}  // namespace dsp
