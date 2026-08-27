// source/dsp/biquad.h — stateful Direct Form II Transposed biquad.
//
// Stateful companion to filter_coeff.h's `rbjBandpassCoeffs`/`BiquadCoeffs`,
// mirroring one_pole_filter.h's split: the coefficient function computes
// {b0,b2,a1,a2} from a cutoff and Q (typically once per block); this class
// holds the running filter state across samples and applies those
// coefficients. `b1` is always 0 for the constant-peak-gain bandpass form
// filter_coeff.h produces, so it is dropped from both the struct and the
// recursion below rather than carried as a dead multiply-by-zero.
//
// Built to replace the Chamberlin `low_`/`band_` state pair in
// wah.cpp/funk_machine_envelope_filter.cpp/harmonica.cpp -- same state
// count (2 floats), so no per-effect state growth from the swap. See
// filter_coeff.h's warning on `svfF1` for why the swap happened: the
// Chamberlin SVF's actual resonant peak drifts sharply from its target as Q
// drops (measured up to 172.3 cents), while this form measures well under
// 0.1 cents error across the same fc/Q sweep.
//
// State-count parity is not the only property that changed. Chamberlin's
// `low`/`band` state are themselves meaningful filtered-signal values,
// independent of the current coefficients -- a live coefficient change
// (all three callers recompute coefficients on a running filter, once per
// block for wah/harmonica, every 8 samples for funk_machine) just continues
// the recursion under new math. This class's `s1`/`s2`, by contrast, encode
// future-output contributions *under the current coefficients* -- a
// coefficient change reinterprets old-coefficient state through new
// coefficients, which can inject a transient a coefficient-independent
// topology wouldn't have. This is a real, well-known category of DSP
// concern for Direct Form structures under live modulation. Measured
// (Python model of both structures under each effect's real update
// pattern, comparing post-jump output against a reference filter already
// running at the new coefficients): this form's transient is not
// systematically worse than the old Chamberlin's own coefficient-update
// transient at these three effects' actual update rates -- smaller in most
// tested scenarios, slightly larger in one (funk_machine's 8-sample rate).
// See the 2026-08-25 addendum in docs/wah-build-walkthrough.md for the
// scenario-by-scenario numbers. Documented here rather than mitigated in
// code (e.g. coefficient smoothing/crossfade) because the measurement
// didn't show a regression versus what already shipped; revisit if a
// faster-than-block-rate consumer is ever added.
//
// Recurrence (Direct Form II Transposed, b1=0):
//   y[n]  = b0*x[n] + s1
//   s1'   = -a1*y[n] + s2
//   s2'   = b2*x[n] - a2*y[n]
//
// CPU cost: 2 multiplies computing y, 4 more updating state -- comparable
// to the 3-multiply Chamberlin recursion it replaces, still no libm call
// per sample. Coefficients are the caller's responsibility to compute and
// pass in (see filter_coeff.h) -- this class only holds and steps state.

#pragma once

#include "filter_coeff.h"

namespace dsp
{

class BandpassBiquad
{
public:
    float process(float input, const BiquadCoeffs& c)
    {
        const float output = c.b0 * input + s1_;
        s1_ = -c.a1 * output + s2_;
        s2_ = c.b2 * input - c.a2 * output;
        return output;
    }

    void reset()
    {
        s1_ = 0.0f;
        s2_ = 0.0f;
    }

private:
    float s1_ = 0.0f;
    float s2_ = 0.0f;
};

}  // namespace dsp
