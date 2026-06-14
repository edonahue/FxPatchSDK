// source/dsp/fractional_delay.h — linear-interpolated reads from a delay buffer.
//
// Harvested from effects/chorus.cpp:172-177; the same idiom recurs in
// harmonica.cpp and bbe_sonic_stomp.cpp.
//
// Linear interpolation is the simplest fractional-delay scheme: two reads,
// a multiply, an add. The frequency-response cost is a mild low-pass
// roll-off at high frequencies relative to the rate of change of the
// delay; acceptable for chorus, vibrato, and short delay lines. For higher
// quality interpolation (pitch shifting, long-time-stretch granular) a
// windowed-sinc primitive is the right tool; sthompsonjr's
// `WindowedSincInterpolator.h` is the canonical reference but unlicensed
// and not portable into this repo.
//
// CPU cost: two memory loads, one multiply, two adds per call.

#pragma once

namespace dsp
{

// Wrapping linear-interpolated read from a buffer of `len` samples. The
// fractional read position is wrapped modulo `len`.
inline float lerpRead(const float* buf, int len, float readPos)
{
    if (readPos < 0.0f) readPos = 0.0f;
    const int   idx0 = static_cast<int>(readPos) % len;
    const int   idx1 = (idx0 + 1) % len;
    const float frac = readPos - static_cast<float>(static_cast<int>(readPos));
    return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
}

// Faster variant for power-of-two-length buffers — pass `mask = len - 1`.
inline float lerpReadPow2(const float* buf, int mask, float readPos)
{
    if (readPos < 0.0f) readPos = 0.0f;
    const int   base = static_cast<int>(readPos);
    const int   idx0 = base & mask;
    const int   idx1 = (base + 1) & mask;
    const float frac = readPos - static_cast<float>(base);
    return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
}

}  // namespace dsp
