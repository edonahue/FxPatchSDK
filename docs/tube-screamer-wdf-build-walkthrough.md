# Tube Screamer-Inspired WDF Sibling — Build Walkthrough

**File:** `effects/tube_screamer_wdf.cpp`
**Date:** 2026-04-19
**Filter/algorithm type:** WDF-style diode-pair overdrive with body/edge split, bounded tone shaping, and output trim
**Reference circuit / inspiration:** Ibanez TS808 Tube Screamer with a TS9 alternate voice
**Primary analysis source:** <https://electrosmash.com/tube-screamer-analysis>
**Variant context:** <https://www.analogman.com/tshist.htm>
**WDF context:** [`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md)
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch is the WDF-style sibling to `effects/tube_screamer.cpp`. Unlike the Big Muff
sibling, it makes one deliberate control-layout change: the Right lane becomes `Level`
instead of `Tone`. That was intentional. The repo's recent drive work showed that output
controls on Endless can feel much less pedal-like than the clipping stages themselves, so
this sibling tests two things at once:

1. whether a wave-solved diode-pair core improves the overdrive feel
2. whether expression-on-level is a better Endless interpretation than expression-on-tone

---

## Circuit Reference

The Tube Screamer family is not just "soft clipping." The important blocks are:

1. low-end trimming before clipping
2. an op-amp-driven nonlinear stage
3. post-clip tone shaping
4. output level control

The existing hand-tuned patch already captured the general mid-hump story, but this sibling
rebuilds the nonlinear center with a local WDF-style diode pair. It does not attempt a full
feedback-network recreation or an exact JRC4558 model. The point is to see whether a more
circuit-shaped clipper plus a more literal output control feel closer to a pedal on Endless
hardware.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 1-pole HP, 1-pole LP, wave-solved antiparallel diode pair, output trim |
| Circuit-modeling approach | WDF-style clip core inside a hybrid hand-authored overdrive |
| Working buffer needed? | No |
| State variable count | 8 floats of filter/clipper state across two channels |
| Knob 0 (Left) | `Drive` |
| Knob 1 (Mid) | `Tone` |
| Knob 2 (Right) / Exp pedal | `Level` |
| Active LED color | `Color::kLightGreen` or `Color::kPastelGreen` |
| Bypassed LED color | `Color::kDimGreen` or `Color::kDarkLime` |
| Working buffer size needed | N/A |

---

## Decision 1 — Overdrive Topology

**Goal:** make the drive patch feel more obviously like a Tube Screamer family overdrive at
realistic input level, without importing a large external WDF framework into the repo.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| keep the original split-band `tanhf` design only | already fits repo style and works | does not test the WDF hypothesis |
| full feedback-network / op-amp port | more schematic fidelity | heavier math and much more infrastructure for one experiment |
| hybrid WDF-style diode-pair core | targets the most identity-defining nonlinear block while keeping the patch understandable | still not a complete circuit port |

**Chose:** hybrid WDF-style diode-pair core. The clipping stage is where the Tube Screamer
family identity is most likely to benefit from a more explicit model.

---

## Decision 1.5 — Circuit Modeling Fidelity

This patch deliberately uses the middle fidelity tier:

- more structured than hand-fit filters plus `tanhf`
- lighter than a full named-circuit WDF implementation
- local enough that the repo can throw it away if the listening result is not better

Expected CPU pressure:

- per sample: 2 channels × 1 clip stage × 4 Newton iterations
- per sample, outside the solver: one HP split, one LP split, one post-tone LP, one `tanhf`
- per block: none

This is materially heavier than the original `tube_screamer.cpp`, but still small enough to
justify as a direct sibling experiment.

---

## Decision 2 — Gain Staging / Normalization

The signal path is intentionally staged rather than brute-forced:

1. `preHp_` trims low end before clipping
2. `splitLp_` separates body from edge so the clip stage can stay focused
3. the diode-pair solver sees the op-amp-shaped input
4. a recovered nonlinear stage rebalances clipped content with some surviving body
5. `toneLp_` provides a bounded post-clip tone split
6. `outputTrim` is tuned so noon lands near unity and the upper half still adds real level

The key repo lesson applied here is that output control must move actual loudness, not only
push harder into the safety limiter.

---

## Decision 3 — Control Layout

This sibling intentionally diverges from `tube_screamer.cpp`.

Reason: the original patch was the repo's case study in preserving the classic `Drive` /
`Level` / `Tone` story while putting expression on `Tone` because param `2` was fixed. The
new sibling answers the other question: what if the Right lane is reserved for the real
output control instead?

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Drive` | edge-of-breakup to stronger clipped push | `0.52f` |
| Mid | 1 | `Tone` | darker to brighter post-clip voicing | `0.50f` |
| Right / Exp | 2 | `Level` | near-unity to boosted output | `0.58f` |

---

## Decision 4 — Parameter Taper / Mapping

### `Drive`

- uses an eased `driveCurve` so more of the sweep stays musically active
- increases both nonlinear strength and the pre-clip/op-amp drive relationship
- target behavior: the patch should stop reading like "mostly louder clean tone"

### `Tone`

- uses a bounded post-clip split driven by `toneCurve`
- heel should stay dark but usable
- toe should brighten the upper mids without turning brittle

### `Level`

- uses a squared/eased `levelCurve`
- is explicitly tuned so analyzer unity position lands near the middle
- expression on the Right lane should feel like a real pedal output swell, not a tone-wah substitute

---

## Decision 4.5 — Primitive Reuse

This patch also keeps its helpers local for now:

- the repo does not yet have a shared `source/dsp/` layer
- the local `WdfAntiparallelDiodePair` is part of the experiment, not a settled abstraction
- a future extraction should wait until the sibling has justified itself by ear and under tests

The reuse lesson here is procedural rather than structural: land the experiment first, then
extract the durable parts.

---

## Decision 5 — Footswitch UX

- `kLeftFootSwitchPress`: bypass toggle
- `kLeftFootSwitchHold`: toggles TS808 / TS9 family voicing

The voice toggle uses `ts9Morph_` instead of a hard branch swap, so the change can be made
without wiping state or causing an obvious pop.

---

## Decision 6 — LED Design

| State | Color | `Color` enum |
|---|---|---|
| TS808 active | Light green | `Color::kLightGreen` |
| TS808 bypassed | Dim green | `Color::kDimGreen` |
| TS9 active | Pastel green | `Color::kPastelGreen` |
| TS9 bypassed | Dark lime | `Color::kDarkLime` |

---

## Decision 7 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | removes stale filter/clipper memory |
| Active -> bypass | No | pass-through path is already stable |
| TS808 / TS9 toggle | No | smoothed morph handles the family shift cleanly |
| Ordinary knob move | No | live sweeps should remain continuous |

---

## Implementation Notes

### Working buffer allocation

No working buffer is required. The patch uses only small per-channel filter and clipper state.

### CPU budget estimate

This sibling is lighter than `big_muff_wdf.cpp` because it has only one solver-based clip
stage per channel, not two. It is still heavier than the original `tube_screamer.cpp`, so it
should be judged as a tonal/control experiment, not assumed to be the new default architecture.

### Output-control target

The analyzer target for this patch was explicit: unity should land around `0.50` on the
Right lane, and the upper half of the control should increase audible loudness without simply
flattening into a ceiling. That requirement drove the final `levelCurve` and `outputTrim`
shape more than schematic literalism did.

---

## Testing

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
```

**Manual listening checklist:**

- [ ] `Drive` becomes audibly more nonlinear across most of the sweep at realistic guitar input
- [ ] `Level` behaves like a practical post-drive output control around and above noon
- [ ] `Tone` stays useful as a normal knob even though expression no longer owns it
- [ ] TS808 mode sounds slightly smoother and rounder than TS9
- [ ] TS9 mode sounds tighter and brighter without leaving the Tube Screamer family
- [ ] compared with `effects/tube_screamer.cpp`, this sibling sounds more "pedal-like" rather than merely different

---

## Future Improvements

1. Measure whether the WDF-style sibling consistently wins on hardware before extracting any
   shared nonlinear primitive.
2. If the output-control result is preferred, reconsider whether `tube_screamer.cpp` should
   keep expression on `Tone` or remain the "classic layout under current wrapper limits" variant.
3. If the clip texture wins but CPU cost is too high, prototype a cheaper approximation that
   preserves the same control-law lessons.
