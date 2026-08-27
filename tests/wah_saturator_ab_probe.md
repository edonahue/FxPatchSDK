# wah.cpp growl/output saturator: tanh vs. rational A/B — result

**Date:** 2026-08-24
**Status:** measured, declined. `effects/wah.cpp` still uses `tanhf`.

## Why this was investigated

`docs/wah-build-walkthrough.md`'s 2026-08-24 addendum found that
`kResonanceGain`'s *actual* value was 5.6 (not the 2.8 the original code
comment claimed), meaning the growl and output-limiter `tanhf` stages were
already saturating harder and more often than the original design intended.
Given the corpus study found no third-party patch links a newlib
transcendental, and `Malleus_Fuzz` uses a cascaded `x/(1+|x|)` rational
saturator instead of `tanhf`, the obvious next move was to try the same swap
here — matching the pattern already applied (and *not* applied, for
`funk_machine_envelope_filter.cpp`'s cold limiter) elsewhere this session.

## Method

Both `tanhf(x*d)/d` and the candidate `x/(1+d*|x|)` share the same
near-origin slope (1.0) and the same asymptote (`1/d`) for a given drive `d`
— they differ only in the *shape* of the transition between those two
regimes. The candidate was substituted for both call sites
(`growlL`/`growlR` and the output-limiter clamp), built through the real ARM
toolchain, and compared against the shipped `tanhf` version with
`scripts/analyze_effects.py` at identical settings (default Q, mix, and a Q
sweep) — not by inspection or intuition.

## Result

| metric | tanh (shipped) | rational (candidate) |
|---|---|---|
| `spectral.thd_percent` | 0.044% | **1.65%** (37x) |
| `nominal_input.active.sine.residual_ratio` | 0.000444 | 0.016506 (37x) |
| `low_input.active.sine.residual_ratio` | 0.000055 | 0.005995 (109x) |

426 of 865 analyzed fields moved. No NaN/Inf, no crash, no qualitative flag
change — this is not a stability finding. It is a clean, consistent,
measurable increase in harmonic content: the rational form's harder knee
adds more high-order harmonics at the same drive setting than `tanhf`'s
smoother saturation curve.

## Conclusion

**Declined.** Both absolute THD figures are still small in isolation
(1.65% is not harsh by drive-pedal standards), but a 37–109x, fully
reproducible increase is not the "negligible delta" bar this investigation
was scoped to. `effects/wah.cpp` keeps `tanhf`. The image-size cost of
keeping it (~850 bytes over the rational variant, from newlib's
`tanhf`/`expm1f`) is small against the 512 KB budget and buys a measurably
cleaner saturation curve.

This is the second time this session a saturator swap suggested by the
corpus study was measured and declined rather than assumed —
`funk_machine_envelope_filter.cpp`'s case was "the nonlinearity is never
called" (cold limiter); this one is "the nonlinearity is called constantly,
and changing it audibly changes the tone." Different reasons, same
discipline: measure, then decide.

## Reproducing

The A/B was done by temporarily substituting the rational form at both call
sites in `effects/wah.cpp`, rebuilding, and diffing
`scripts/analyze_effects.py`'s output against a captured baseline — no
standalone probe binary was kept, since the substitution only needs to exist
for the duration of the comparison and the result is what's worth keeping.
To reproduce: replace `tanhf(x * kGrowlDrive) * kGrowlInv` with
`x / (1.0f + kGrowlDrive * fabsf(x))` (and the equivalent for the output
stage with `kOutDrive`), run `bash tests/check_arm_build.sh` then
`python3 scripts/analyze_effects.py`, and compare `spectral.thd_percent`
against a pre-change capture.

## See also

- [`../docs/wah-build-walkthrough.md`](../docs/wah-build-walkthrough.md) —
  the 2026-08-24 addendum this follows on from
- [`funk_machine_limiter_probe.cpp`](funk_machine_limiter_probe.cpp) — the
  precedent for measuring-then-declining a saturator swap, for a different
  reason (there, the nonlinearity was never engaged at all)
- [`../docs/reverse-engineering/factory-patch-idioms.md`](../docs/reverse-engineering/factory-patch-idioms.md)
  — the corpus finding (`x/(1+|x|)`) this investigation started from
