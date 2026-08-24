// source/dsp/soft_limit.h — symmetric soft-clip with linear knee + tanh tail.
//
// Harvested from effects/tube_screamer.cpp:49-59; the same idiom recurs in
// the corpus with the per-effect tuning varying slightly: tube_screamer
// uses (0.90, 0.25, 0.10), tube_screamer_wdf uses (0.90, 0.22, 0.10),
// big_muff_wdf uses (0.92, 0.24, 0.08). All three parameters — threshold,
// divisor, and the tanh-tail amplitude — are exposed so per-effect tuning
// lives at the call site instead of in a near-duplicate copy.
//
// Shape: |x| <= threshold passes through unchanged. Above threshold, the
// overshoot maps through tanh; the asymptote is `threshold + tail`. The
// defaults (0.90, 0.25, 0.10) give a smooth approach to +/-1.0f. A caller
// changing threshold should usually pass `tail = 1.0f - threshold` to
// preserve the ±1.0 envelope.
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
                       float divisor   = 0.25f,
                       float tail      = 0.10f)
{
    // __builtin_ rather than fabsf: COMMON_FLAGS carries -fno-builtin, so the
    // plain name is a `bl` into libm where the builtin is a single vabs.f32.
    // Exact either way -- fabs only clears the sign bit. See dsp/clamp.h.
    const float absValue = __builtin_fabsf(value);
    if (absValue <= threshold)
    {
        return value;
    }
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    const float over = (absValue - threshold) / divisor;
    return sign * (threshold + tail * tanhf(over));
}

}  // namespace dsp
