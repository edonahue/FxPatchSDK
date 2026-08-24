// source/dsp/clamp.h — branchless range clamps built on the FPU's min/max.
//
// Harvested from the file-local helpers in effects/*.cpp: ten effects defined
// clamp01, eight defined clampUnit and two defined clampSigned, and six of the
// clamp01 bodies were byte-identical. Same situation that produced
// one_pole_filter.h.
//
// Why __builtin_ and not the obvious spellings:
//
//   if (v < 0) return 0; if (v > 1) return 1; return v;   -> 6 insns + a branch
//   fminf(fmaxf(v, 0.0f), 1.0f)                           -> two library CALLS
//   __builtin_fminf(__builtin_fmaxf(v, 0.0f), 1.0f)       -> vmaxnm + vminnm
//
// The middle line is the trap. The Makefile's COMMON_FLAGS include
// -fno-builtin, so plain fminf/fmaxf do not get folded into instructions --
// they become real calls into libm, which is worse than the if-chain we
// started with. Only the explicit builtins produce the FPv5 vmaxnm/vminnm
// pair. Measured with the real build flags; see
// docs/reverse-engineering/factory-patch-idioms.md, Finding C.
//
// This is also what Polyend's own patches do: 142 vmaxnm/vminnm across the
// five factory plates, and zero across all fourteen of ours before this.
//
// NaN behaviour differs from the if-chain these replace, and the difference is
// an improvement rather than a regression. The if-chain propagates NaN (both
// comparisons are false, so the input falls through). vmaxnm/vminnm implement
// IEEE-754 maxNum/minNum, which return the non-NaN operand -- so a NaN that
// reaches clamp01 comes out as 0.0f, bounded, instead of propagating into the
// output buffer. Two consequences worth knowing: a NaN is contained rather
// than escaping to the DAC, and it no longer trips the NaN detector in
// tests/effect_probe.cpp at the point of clamping. The detector still sees any
// NaN produced outside a clamp, which is where they actually originate.
//
// CPU cost: two instructions, no branch, no call.

#pragma once

namespace dsp {

// Clamp to [0, 1] — the normalized parameter range every knob uses.
constexpr float clamp01(float value)
{
    return __builtin_fminf(__builtin_fmaxf(value, 0.0f), 1.0f);
}

// Clamp to [-limit, +limit]. `limit` is expected to be positive.
constexpr float clampSigned(float value, float limit)
{
    return __builtin_fminf(__builtin_fmaxf(value, -limit), limit);
}

// Clamp to [-1, +1] — the output range the SDK requires.
constexpr float clampUnit(float value)
{
    return clampSigned(value, 1.0f);
}

} // namespace dsp
