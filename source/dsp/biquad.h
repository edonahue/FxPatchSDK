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
// drops (measured up to 174 cents), while this form measures 0.00 cents
// error across the same fc/Q sweep.
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
