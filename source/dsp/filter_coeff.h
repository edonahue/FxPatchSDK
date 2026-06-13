// source/dsp/filter_coeff.h — one-pole low-pass / high-pass coefficient helpers.
//
// Harvested from effects/tube_screamer.cpp:61-70; the same idiom recurs
// identically in 8 of the 12 effects. One-pole IIRs are the workhorse of
// this corpus: cheap, well-behaved at 48 kHz single precision, easy to
// reason about, and adequate for the broad voicing filters most pedal-style
// effects need before or after a clipper.
//
// The returned coefficient is the "alpha" used in a standard 1-pole IIR
// recurrence. The exact recurrence form depends on the call site; see
// effects/tube_screamer.cpp for the canonical examples.
//
// CPU cost: a couple of multiplies and a divide per call. Cache the
// coefficient and recompute only when the cutoff parameter changes.
//
// Reference shape: DaisySP's `OnePole` has the same structure; this is an
// independent implementation written for the Endless.

#pragma once

#include "../Patch.h"

namespace dsp
{

inline float lpCoeff(float fc, float fs = static_cast<float>(Patch::kSampleRate))
{
    constexpr float kTwoPi = 6.283185307f;
    const float omega = kTwoPi * fc / fs;
    return omega / (1.0f + omega);
}

inline float hpCoeff(float fc, float fs = static_cast<float>(Patch::kSampleRate))
{
    constexpr float kTwoPi = 6.283185307f;
    return 1.0f / (1.0f + kTwoPi * fc / fs);
}

}  // namespace dsp
