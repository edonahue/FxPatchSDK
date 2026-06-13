// source/dsp/crossfade.h — equal-power and linear mix laws.
//
// Harvested from effects/back_talk_reverse_delay.cpp:72-84; the same pair
// recurs in big_muff.cpp and big_muff_wdf.cpp.
//
// Equal-power: dry^2 + wet^2 = 1 at any mix position, so perceived
// loudness stays roughly constant across the knob sweep. Use for dry/wet
// blends on chorus, delay, reverb, and most modulation effects.
//
// Linear: dry + wet = 1. Simpler, but louder at mix = 0.5 by about 3 dB.
// Use only when one source dominates and a straight crossfade is what you
// want.
//
// CPU cost: equalPower is two trig calls per change of mix — cache when
// the mix knob is steady. linear is a single subtract.

#pragma once

#include <cmath>

namespace dsp
{

struct MixGains
{
    float dry;
    float wet;
};

inline MixGains equalPower(float mix)
{
    constexpr float kHalfPi = 1.57079632679f;
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    return { cosf(mix * kHalfPi), sinf(mix * kHalfPi) };
}

inline MixGains linear(float mix)
{
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    return { 1.0f - mix, mix };
}

}  // namespace dsp
