// source/dsp/fractional_delay.h — linear-interpolated reads from a delay buffer.
//
// Harvested from effects/chorus.cpp:172-177. The doc comment on lerpRead()
// has always claimed the read position "is wrapped modulo len" for a
// negative input, but the implementation only ever clamped it to 0 --
// three effects (chorus.cpp, harmonica.cpp, bbe_sonic_stomp.cpp) each
// independently discovered this gap and worked around it themselves
// (chorus.cpp pre-wraps its readPos before calling in; harmonica.cpp and
// bbe_sonic_stomp.cpp each carry their own inline reimplementation with
// correct wrap-around instead of calling this helper at all). Fixed here
// so the function matches its own documented contract and none of the
// three callers need a workaround anymore -- verified this changes
// nothing for chorus.cpp specifically, since it never passed a negative
// value in to begin with (its own pre-wrap already normalized it).
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
// CPU cost: two memory loads, one multiply, two adds per call, plus a
// bounded loop for the (rare -- normal modulation depths swing at most a
// fraction of one buffer length) case of a very negative readPos.

#pragma once

namespace dsp
{

// Wrapping linear-interpolated read from a buffer of `len` samples. The
// fractional read position is wrapped modulo `len`, including for
// negative input -- e.g. a sine-modulated tap that swings past the start
// of the buffer wraps to the end, rather than sticking at index 0.
inline float lerpRead(const float* buf, int len, float readPos)
{
    while (readPos < 0.0f) readPos += static_cast<float>(len);
    const int   idx0 = static_cast<int>(readPos) % len;
    const int   idx1 = (idx0 + 1) % len;
    const float frac = readPos - static_cast<float>(static_cast<int>(readPos));
    return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
}

// Faster variant for power-of-two-length buffers — pass `mask = len - 1`.
// Currently unused by any effect in this corpus. Unlike lerpRead() above,
// this does NOT correctly wrap a negative readPos: static_cast<int> on a
// negative float truncates toward zero rather than flooring, so both
// `base` and `frac` come out wrong for non-integer negative input, and a
// bitmask AND alone doesn't fix that upstream error. Fix this properly
// (floor readPos before splitting base/frac) if/when an effect actually
// needs a negative-going power-of-two-length read -- not fixed
// speculatively here since nothing currently exercises it.
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
