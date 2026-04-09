# MXR Distortion+ Circuit Analysis & Endless DSP Model

## Overview

The MXR Distortion+ (MXR M104) is one of the canonical single-op-amp distortion pedals:
input filtering, a gain stage, diode clipping, a simple treble rolloff, and output level.
That simplicity makes it a useful circuit-to-DSP teaching case.

This repository's `effects/mxr_distortion_plus.cpp` keeps that broad topology and the
germanium-style clipping character, but it no longer copies the original pot law directly.
On Endless hardware, the literal Distortion+ gain formula made the left knob feel mostly
inactive until the final quarter-turn, and the earlier tone sweep was too subtle on guitar.
The current patch is therefore best understood as a **usable Endless-tuned Distortion+ clone**.

## Hardware Review Retrospective

This patch is now the main control-law case study in the repository.

The earlier Endless version failed in three specific ways during guitar testing:

- the Distortion knob behaved like it had a dead zone for most of its sweep, then jumped late
- the Tone knob technically changed the low-pass cutoff, but did too little in a guitar-useful range
- the Level knob became hard to use because the gain stage already made the patch much louder at high drive

None of those problems were syntax or DSP-topology bugs. They were **control-law bugs**:
the analog-inspired formulas were too literal for the Endless control surface.

## Original Circuit Topology

```text
Input -> HP filter -> LM741 gain stage -> 1N270 diode clip -> LP tone filter -> Level -> Output
```

Original analog blocks and current DSP counterparts:

| Block | Original analog idea | Current DSP stage |
|---|---|---|
| Input filtering | coupling capacitor plus distortion-dependent resistance | 1-pole IIR high-pass |
| Gain | LM741 non-inverting amplifier | scalar pre-clip gain |
| Clipping | anti-parallel 1N270 germanium diodes | `tanhf()` soft clip |
| Tone | passive low-pass after clipping | 1-pole IIR low-pass |
| Level | output pot | scalar output gain |

## What The Patch Preserves

- Distortion still changes both drive and low-end tightness.
- Clipping is still smooth and symmetric, aimed at a germanium-like knee.
- Tone is still a post-clip treble rolloff, not a full EQ.
- Level is still the final output trim, and remains the expression target because this repo
  routes expression to param `2`.

## What The Patch Adapts For Endless

### Distortion

The original Distortion+ uses:

```text
gain = 1 + 1M / (4.7k + RV)
```

where the same pot also pushes the input high-pass cutoff upward. That produces an enormous
range, from about `2x` to `213x`, with most of the audible change stacked near the end of the
knob travel.

The current patch keeps the coupling concept but replaces the raw pot law with a smoother
control curve:

```cpp
float driveCurve = std::pow(dist_, 0.75f);
float gain = 1.5f + 16.0f * driveCurve;
```

Practical result:

- low settings now add audible grit instead of staying nearly clean
- the full sweep is usable with guitar
- maximum drive is still clearly in Distortion+ territory, but without the runaway jump

### Low-End Tightening

The hardware raises the input high-pass cutoff dramatically as distortion increases. That is a
real part of the pedal's sound, but the literal analog range is too extreme for this patch.

The current Endless mapping is:

```cpp
float fc_hp = 35.0f + 380.0f * dist_ * dist_;
```

So the left knob still tightens bass as drive rises, but in a controlled range of roughly
`35 Hz` to `415 Hz` instead of the full analog swing to about `720 Hz`.

### Tone

The earlier SDK version used a very open low-pass sweep:

```cpp
3000 Hz -> 20000 Hz
```

On guitar, that often sounded like "almost no tone change." The current patch moves the tone
control into a more obviously useful range:

```cpp
float toneCurve = std::pow(tone_, 1.25f);
float fc_lp = 800.0f + 7200.0f * toneCurve;
```

That makes the middle knob act like a real dark-to-bright voicing control instead of a mostly
inaudible anti-fizz adjustment.

### Level

The original design is just an output pot. In the first Endless version, that meant the right
knob became hard to use once distortion got high because the patch was already much denser and
louder.

The current level stage adds two Endless-specific decisions:

```cpp
float levelCurve = level_ * (0.5f + 0.5f * level_);
float outputTrim = 1.15f - 0.45f * driveCurve;
float out = clippedAndFiltered * levelCurve * outputTrim;
```

- the level knob uses a gentler taper
- output is compensated downward as distortion rises

That keeps the right knob practical across the full drive sweep while preserving enough level
range for matching or pushing the bypassed signal.

## Hardware Lessons Learned

The MXR patch establishes a few practical rules for future patches in this fork:

1. Do not assume the original analog pot law should be copied directly to a normalized `0.0-1.0` knob.
2. Treat "does this control do useful work across most of its travel?" as a primary design question.
3. If one knob changes both saturation and loudness, add output compensation early instead of treating it as a later polish pass.
4. Prefer a musically useful default setting over a numeric midpoint.

This is why the repository now treats taper and default-value review as part of patch design,
not just implementation detail.

## Current DSP Signal Chain

Per sample, per channel:

1. High-pass filter to manage low-end build-up as distortion increases
2. Pre-clip gain stage
3. `tanhf()` soft clipping for germanium-like saturation
4. Low-pass tone filter
5. Level stage with distortion-dependent output compensation

Coefficient and control calculations are still done once per audio buffer, not once per sample.
That keeps the inner loop small and matches the rest of this repo's DSP practice.

## Expression Pedal Behavior

This repository routes the expression pedal to param `2` globally in
`internal/PatchCppWrapper.cpp`. For this patch, that means:

- Right knob = `Level`
- Expression pedal = `Level`
- When the expression pedal is connected, the physical Right knob is ignored

This is a good fit for the MXR patch because output level is a performance control. It works
well for rhythm/lead balancing or for taming a brighter, higher-drive setting in real time.

## Tradeoffs and Limitations

| Area | Current choice | Reason |
|---|---|---|
| Gain law | Endless-tuned curve, not literal analog formula | better control sweep on hardware |
| Bass cut | reduced range vs stock circuit | keeps high-gain settings usable |
| Tone | guitar-focused LP range | makes the middle knob clearly audible |
| Diode model | `tanhf()` | simple, stable, stock-SDK-friendly |
| Oversampling | none | consistent with repo constraints, but aliasing can still appear at extreme settings |
| Op-amp behavior | no explicit LM741 saturation or slew model | clipping diodes dominate the audible result here |

If future work needs stricter analog fidelity, a Wave Digital Filter or oversampled staged clipper
would be the next escalation path. That is outside the current stock-SDK-friendly implementation.

## Parameter Mapping Summary

| Control | SDK param | Current behavior |
|---|---|---|
| Left knob | `0` | `Distortion`: smoother drive plus low-end tightening |
| Mid knob | `1` | `Tone`: low-pass from about `800 Hz` to `8 kHz` |
| Right knob / expression | `2` | `Level`: tapered output gain with drive compensation |

Defaults in the current patch:

- Distortion: `0.35`
- Tone: `0.55`
- Level: `0.65`

These defaults are chosen to land near a moderate, immediately usable guitar setting rather
than at an arbitrary midpoint.

## Follow-Up Checks For Existing Patches

MXR is the patch that clearly required retuning, but the same review lens should be applied
to the rest of the repo:

- `chorus.cpp`: current mapping looks sound; verify Depth still feels useful near both extremes
- `wah.cpp`: current log sweep looks right; verify the Q control stays musical across the full range
- `bbe_sonic_stomp.cpp`: current bounded blend/offset controls look intentional; verify each knob produces a distinct audible shift without bunching near the ends

At the time of this write-up, those are watch-items, not confirmed retune requirements.

## References

- ElectroSmash — MXR Distortion+ Analysis: <https://www.electrosmash.com/mxr-distortion-plus-analysis>
- 1N270 germanium diode datasheets for soft-knee forward-voltage behavior
- LM741 datasheets for the original non-inverting amplifier topology
- Yeh & Abel, "Simplified, Physically-Informed Models of Distortion and Overdrive Guitar Effects Pedals," DAFX 2007: <https://ccrma.stanford.edu/~dtyeh/papers/yeh07_dafx_distortion.pdf>
- Distorque Audio Plusdistortion notes as a practical software reference point for Distortion+ style modeling: <http://distorqueaudio.com/plugins/plusdistortion.html>
- General methodology: [`docs/circuit-to-patch-conversion.md`](circuit-to-patch-conversion.md)

## Related Files

- `effects/mxr_distortion_plus.cpp`
- `docs/circuit-to-patch-conversion.md`
- `internal/PatchCppWrapper.cpp`
