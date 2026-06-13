// source/dsp/soft_limit.h — symmetric soft-clip with linear knee + tanh tail.
//
// Harvested from effects/tube_screamer.cpp:49-59; the same idiom recurs in
// 8 of the 12 effects with the divisor varying slightly: 0.25f is the
// canonical value, tube_screamer_wdf uses 0.22f, big_muff_wdf uses 0.24f.
// The threshold and divisor are parameters here so per-effect tuning lives
// at the call site instead of in a near-duplicate copy.
//
// Shape: |x| <= threshold passes through unchanged. Above threshold, the
// overshoot maps through tanh; the asymptote is `threshold + 0.1f`, so the
// default 0.90f threshold yields a smooth approach to +/-1.0f. Callers that
// change the threshold are choosing a different asymptote.
//
// Use this as a final safety stage just before output, not as a creative
// clipper. For creative non-linearity, use the per-effect drive curves,
// diode pairs, or asymmetric clippers each effect already implements.
//
// CPU cost: a single tanhf only on the overshoot path; the linear region
// is a compare and a return.

#pragma once

#include <cmath>

namespace dsp
{

inline float softLimit(float value,
                       float threshold = 0.90f,
                       float divisor   = 0.25f)
{
    const float absValue = fabsf(value);
    if (absValue <= threshold)
    {
        return value;
    }
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    const float over = (absValue - threshold) / divisor;
    return sign * (threshold + 0.10f * tanhf(over));
}

}  // namespace dsp
