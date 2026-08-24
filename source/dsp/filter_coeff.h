// source/dsp/filter_coeff.h — filter coefficient helpers.
//
// One-pole low/high-pass: harvested from effects/tube_screamer.cpp:61-70;
// the same idiom recurs identically in 8 of the 14 effects. One-pole IIRs
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
// identical `f1 = 2*sin(pi*fc/fs)` formula. `q1 = 1/Q` stays a one-line
// inline computation at each call site.
//
// WARNING -- do not use this for a resonant/swept bandpass. Driving the
// actual per-sample recursion with swept sine tones and measuring where the
// output truly peaks (not where the pole angle points -- those diverge for
// this filter's zero structure at low Q) shows the real resonant frequency
// drifts sharply as Q drops: up to 174 cents (1.7 semitones) sharp at Q=1,
// fc=2500 Hz, verified two independent ways (a C++ probe running the exact
// recursion, and a from-scratch Python reimplementation, agreeing to the
// Hz). This is a documented limitation of the Chamberlin SVF, not a bug
// specific to this codebase -- see Lazzarini & Timoney, "Improving the
// Chamberlin Digital State Variable Filter" (arXiv:2111.05592). All three
// prior consumers (wah.cpp, funk_machine_envelope_filter.cpp,
// harmonica.cpp) were measurably affected at their real operating ranges
// and have since moved to `rbjBandpassCoeffs`/`dsp::BandpassBiquad`
// (source/dsp/biquad.h) below, which measures 0.00 cents error across the
// same sweep. `svfF1` is kept because it is still a correct, tested,
// general-purpose Chamberlin coefficient for a non-resonant use -- but as
// of this fix it has no consumers in effects/.
//
// CPU cost: one sinf() call. Cache and recompute only when fc changes,
// exactly like lpCoeff/hpCoeff above.
//
// RBJ constant-peak-gain bandpass biquad: the correct replacement above.
// Standard form from the Audio EQ Cookbook
// (https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html),
// already cited elsewhere in this repo's own circuit-conversion docs. The
// peak of |H| is exactly 1.0 at fc for any Q, by construction -- unlike the
// Chamberlin SVF's peak (~Q/2, needing a per-call-site `(2/Q)` correction
// that turned out to itself be Q-dependent and wrong). Returns
// already-normalized (divided by a0) coefficients; b1 is always 0 for this
// form, so it is omitted from BiquadCoeffs and the recursion in
// dsp::BandpassBiquad drops that term.
//
// CPU cost: one sinf(), one cosf(), one divide. Cache and recompute only
// when fc or Q changes -- every current caller does this once per block
// (funk_machine_envelope_filter.cpp derives the equivalent libm-free, since
// its control rate is faster than a block; see its own file for why).

#pragma once

#include <cmath>

#include "../Patch.h"

namespace dsp
{

struct BiquadCoeffs
{
    float b0;
    float b2;
    float a1;
    float a2;
};

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

inline BiquadCoeffs rbjBandpassCoeffs(float fc, float q, float fs = static_cast<float>(Patch::kSampleRate))
{
    constexpr float kTwoPi = 6.283185307f;
    const float w0    = kTwoPi * fc / fs;
    const float cosW0 = cosf(w0);
    const float alpha = sinf(w0) / (2.0f * q);
    const float invA0 = 1.0f / (1.0f + alpha);

    return {
        alpha * invA0,           // b0
        -alpha * invA0,          // b2 (b1 is always 0 for this form)
        -2.0f * cosW0 * invA0,   // a1
        (1.0f - alpha) * invA0,  // a2
    };
}

}  // namespace dsp
