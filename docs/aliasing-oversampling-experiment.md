# Aliasing / Oversampling Experiment

**Date:** 2026-06-14.
**Status:** measured, data-driven conclusion below. Does not modify
`effects/tube_screamer_wdf.cpp` — this is a standalone, additive
experiment. Whether to apply oversampling to the shipped effect is a
separate, later decision.

## The question

The six tanh/diode-based drive effects in this corpus (`tube_screamer`,
`tube_screamer_wdf`, `klon_centaur`, `big_muff`, `big_muff_wdf`,
`mxr_distortion_plus`) use naive nonlinear clipping with no anti-aliasing
measure. A nonlinear stage generates harmonics; any harmonic above Nyquist
folds back into the audible band as aliasing. Does 2x-oversampling the
nonlinearity measurably reduce that aliasing, and at what CPU cost?

This experiment measures it on one representative nonlinearity —
`WdfAntiparallelDiodePair::process()` in `effects/tube_screamer_wdf.cpp:78-112`,
a 4-iteration Newton-Raphson solve with 8 `sinhf`/`coshf` calls per sample
— rather than assuming a technique's worth from theory alone, matching
this repo's "measure, don't assume" convention (see
[`docs/cycle-budget.md`](cycle-budget.md)).

## Method

- **Harness:** [`tests/oversample_alias_probe.cpp`](../tests/oversample_alias_probe.cpp).
  `#include`s `effects/tube_screamer_wdf.cpp` directly so the experiment
  tests the exact shipped nonlinearity, not a copy.
- **Three processing modes:**
  - **1x** — baseline, one `process()` call per sample.
  - **2x-naive** — linear-interpolated midpoint upsample, `process()` at
    both the midpoint and the original sample, boxcar `[0.5, 0.5]`
    average decimate. The cheapest possible oversampling.
  - **2x-halfband** — textbook 2x oversampling: zero-stuff upsample,
    filter with a 7-tap multiplierless halfband FIR
    (`[-1, 0, 9, 16, 9, 0, -1] / 32`, exactly −6.02 dB at Nyquist/2),
    process at the 2x rate, filter again (anti-aliasing), decimate by
    dropping every other sample.
- **Two test signals**, both bin-exact for an 8192-sample block at 48 kHz
  (deliberately not simple submultiples of 48000, so aliased energy is
  separable from true harmonics without windowing ambiguity):
  - **Sweep tone** — a single tone at ~5982.66 Hz.
  - **Two-tone** — ~4998.05 Hz + ~6099.61 Hz, a classic intermodulation
    test.
- **Three drive levels**, relative to the clipper's own internal clamps
  (`incident` clamped to ±2.6, `voltage` clamped to ±1.8): mild (0.3),
  moderate (0.9), hard (2.4).
- **Alias-energy metric**: [`scripts/analyze_oversample_alias.py`](../scripts/analyze_oversample_alias.py)
  computes a windowed FFT (a pure-Python radix-2 implementation — no numpy
  dependency, matching `scripts/analyze_effects.py`'s stdlib-only
  convention) and reports `alias_energy_ratio_dB` = energy outside the
  expected fundamental/harmonic (or intermodulation) bins, divided by
  total energy, in dB. Lower (more negative) is better — less energy
  where it shouldn't be.
- **CPU cost**: `tests/oversample_alias_probe.cpp --bench` uses
  `__rdtsc`, min-of-20-trials, `-O3` (matching `cycle-budget.md`'s stated
  build flags) — the host-cycle-ratio methodology `cycle-budget.md`
  itself sanctions as an ordering signal, explicitly not a Cortex-M7
  number.

## Results

Alias-energy reduction, in dB relative to the 1x baseline (more negative
= more reduction; measured 2026-06-14):

| Signal | Drive | 1x baseline | 2x-naive Δ | 2x-halfband Δ |
|---|---|---|---|---|
| Sweep tone | mild | −60.66 dB | −5.64 dB | −11.74 dB |
| Sweep tone | moderate | −33.44 dB | −5.63 dB | −12.72 dB |
| Sweep tone | hard | −19.36 dB | −8.53 dB | −15.77 dB |
| Two-tone | mild | −63.64 dB | −4.75 dB | −9.04 dB |
| Two-tone | moderate | −35.75 dB | −4.47 dB | −8.49 dB |
| Two-tone | hard | −22.27 dB | −4.65 dB | −7.09 dB |

CPU cost (host x86, min-of-20-trials, ordering signal only — **not**
Cortex-M7 numbers):

| Mode | Host cycles/sample | Ratio to 1x |
|---|---|---|
| 1x | 701.98 | 1.00x |
| 2x-naive | 1404.51 | 2.00x |
| 2x-halfband | 1481.61 | 2.11x |

The measured cycle ratios land almost exactly on the a priori estimate
made before running the experiment (~2.0x for naive, ~2.05–2.15x for
halfband, since the doubled transcendental-call cost dominates both the
interpolation and decimation math) — a useful sanity check that the
harness has no bugs, not a coincidence to read anything further into.

## Conclusion

Both oversampling modes measurably reduce aliasing, at every drive level
and for both test signals, with **halfband consistently and substantially
outperforming naive** (roughly 2x more reduction in dB terms — e.g. sweep
tone at hard drive: −8.53 dB naive vs. −15.77 dB halfband). The reduction
is largest at hard drive (more harmonic content generated in the first
place, more opportunity for aliasing) and smallest at mild drive (the
nonlinearity barely engages, so there's little to alias).

The CPU cost is real and roughly doubles the already-nontrivial diode
solve (~2.0–2.1x), for a technique that would need to run on *every
sample of every one of the six drive-family effects* if applied broadly —
not a cheap, obviously-worth-it addition. `WdfAntiparallelDiodePair` is
already one of the more expensive primitives in the corpus (8
transcendental calls per sample); doubling it is a meaningful chunk of a
15k-cycle/sample budget that has **no measured Cortex-M7 data behind it
yet** (see [`docs/cycle-budget.md`](cycle-budget.md)).

**Recommendation: document the technique and the tradeoff (done — see
[`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md)),
but do not apply it to any shipped effect without real hardware cycle
data justifying the cost.** This experiment produced the numbers; it
deliberately does not make the shipping decision. If hardware profiling
ever shows meaningful headroom on a drive-family patch, halfband
oversampling (not naive — the quality gap is too large to justify the
same 2x-ish cost for a materially weaker result) is the technique to
reach for, scoped to the effects where audible aliasing has actually been
reported, not applied blanket-wide.

## Reproducing

```bash
mkdir -p build/oversample_experiment
g++ -std=c++20 -O3 -fno-exceptions -fno-rtti -fsingle-precision-constant \
    -I source tests/oversample_alias_probe.cpp -o build/oversample_probe
build/oversample_probe build/oversample_experiment
python3 scripts/analyze_oversample_alias.py build/oversample_experiment
build/oversample_probe --bench
```

## See also

- [`tests/oversample_alias_probe.cpp`](../tests/oversample_alias_probe.cpp)
- [`scripts/analyze_oversample_alias.py`](../scripts/analyze_oversample_alias.py)
- [`docs/cycle-budget.md`](cycle-budget.md) — the CPU-budget philosophy and
  `__rdtsc` methodology this experiment reuses
- [`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md) —
  the aliasing subsection that cites this experiment
- `effects/tube_screamer_wdf.cpp:78-112` — the nonlinearity under test
