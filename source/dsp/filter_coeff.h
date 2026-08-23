// source/dsp/filter_coeff.h — filter coefficient helpers.
//
// One-pole low/high-pass: harvested from effects/tube_screamer.cpp:61-70;
// the same idiom recurs identically in 8 of the 12 effects. One-pole IIRs
// are the workhorse of this corpus: cheap, well-behaved at 48 kHz single
// precision, easy to reason about, and adequate for the broad voicing
// filters most pedal-style effects need before or after a clipper.
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
//
// Chamberlin SVF coefficient: harvested from effects/wah.cpp:111 and
// effects/funk_machine_envelope_filter.cpp, both of which inlined the
// identical `f1 = 2*sin(pi*fc/fs)` formula -- the repo's own bar for
// source/dsp/ extraction (see CLAUDE.md's "at least two effects would use
// it" policy). `q1 = 1/Q` stays a one-line inline computation at each call
// site; it varies enough by call site (linear Q range, default Q, etc.)
// that a shared helper would just be `1.0f / q`, not worth extracting.
//
// CPU cost: one sinf() call. Cache and recompute only when fc changes,
// exactly like lpCoeff/hpCoeff above.

#pragma once

#include <cmath>

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

inline float svfF1(float fc, float fs = static_cast<float>(Patch::kSampleRate))
{
    constexpr float kPi = 3.14159265f;
    return 2.0f * sinf(kPi * fc / fs);
}

}  // namespace dsp
