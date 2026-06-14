// source/dsp/dc_blocker.h — 1-pole DC-blocking high-pass.
//
// Harvested from effects/harmonica.cpp:360-362; the only site in the
// corpus that needs one because it post-processes the harmonica's
// asymmetric clipper, which produces a non-zero mean signal.
//
// Standard recurrence: y[n] = alpha * (y[n-1] + x[n] - x[n-1]). For alpha
// very close to 1.0 the cutoff approaches DC; harmonica uses alpha ~ 0.999
// for a cutoff near 7 Hz at 48 kHz. Lower alpha gives a higher cutoff and
// a more aggressive DC removal at the cost of low-frequency response.
//
// Use whenever a non-linear stage can leave DC offset on the signal:
// asymmetric clippers, half-wave rectifiers, slew limiters.
//
// CPU cost: one multiply and two adds per sample.

#pragma once

namespace dsp
{

class DcBlocker
{
public:
    void setAlpha(float alpha) { alpha_ = alpha; }

    float process(float x)
    {
        const float y = alpha_ * (yPrev_ + x - xPrev_);
        xPrev_ = x;
        yPrev_ = y;
        return y;
    }

    void reset()
    {
        xPrev_ = 0.0f;
        yPrev_ = 0.0f;
    }

private:
    float alpha_ = 0.999f;
    float xPrev_ = 0.0f;
    float yPrev_ = 0.0f;
};

}  // namespace dsp
