# Big Muff Pi-Inspired Hybrid WDF Fuzz — Build Walkthrough

**File:** `effects/big_muff_wdf.cpp`
**Date:** 2026-04-19
**Filter/algorithm type:** hybrid WDF-style diode-pair clipping with 1-pole tone-stack branches and equal-power blend
**Reference circuit / inspiration:** Electro-Harmonix Big Muff Pi, voiced toward the Ram's Head family with a Tone Bypass-style alternate voice
**Primary analysis source:** <https://www.electrosmash.com/big-muff-pi-analysis>
**Variant context:** <https://www.kitrae.net/music/big_muff_historyB.html>
**WDF context:** [`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md)
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch is the WDF-style sibling to `effects/big_muff.cpp`, not a replacement for it.
The design goal is narrow and testable: keep the repo's current Endless-friendly `Sustain`
/ `Tone` / `Blend` surface, but replace the hand-shaped clip core with a more circuit-like
wave-solved diode-pair path. That lets the repo compare "same controls, different nonlinear
engine" instead of mixing a topology experiment with a UX rewrite.

---

## Circuit Reference

The same Big Muff block view still applies:

1. input conditioning / boost
2. two cascaded clipping stages
3. passive tone stack
4. output recovery

The hand-tuned `big_muff.cpp` patch already captured that audible structure well enough to
sound convincingly Muff-like. The motivation for this sibling was different: the
`sthompsonjr` fork showed that a more explicit nonlinear stage might improve the feel of the
sustain ramp and the density of the clipping without forcing a full transistor-bias model
into this repo. So this patch keeps the block order but swaps the core clip stages for two
wave-solved antiparallel diode-pair elements wrapped in simpler local filtering.

This is intentionally still not a full schematic port. The repo wanted a controlled
experiment, not a whole local WDF framework before any listening result existed.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 1-pole HP, 1-pole LP, wave-solved antiparallel diode pair, equal-power blend |
| Circuit-modeling approach | WDF-style nonlinear core inside a hybrid hand-authored patch |
| Working buffer needed? | No |
| State variable count | 18 floats of filter/clipper state across two channels |
| Knob 0 (Left) | `Sustain` |
| Knob 1 (Mid) | `Tone` |
| Knob 2 (Right) / Exp pedal | `Blend` |
| Active LED color | `Color::kRed` or `Color::kMagenta` |
| Bypassed LED color | `Color::kDarkRed` or `Color::kDimCyan` |
| Working buffer size needed | N/A |

---

## Decision 1 — Core Topology

**Goal:** test whether a more circuit-shaped nonlinear stage improves feel without giving up
the current repo lessons about expression routing and control usefulness.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| keep the original `tanhf`-based patch only | already good, cheap, simple | does not answer whether WDF-style clipping adds anything meaningful |
| full transistor-level Big Muff port | highest schematic fidelity | too much new framework and tuning risk for one experiment |
| hybrid WDF-style diode-pair core | isolates the nonlinear experiment while preserving the rest of the patch story | less literal than a full circuit port |

**Chose:** hybrid WDF-style diode-pair core. The clip stages are where the experiment can
matter most, while the public controls and general tone-stack shape already worked.

---

## Decision 1.5 — Circuit Modeling Fidelity

This patch uses the middle option from the repo template:

- not just hand-tuned filters + `tanhf`
- not yet a full named-circuit WDF port
- a local WDF-style nonlinear core inside an otherwise repo-native patch

The expensive math lives in `WdfAntiparallelDiodePair::process(...)`. Each channel runs two
clip stages, each stage runs four Newton iterations, and each iteration evaluates `sinhf`
plus `coshf`. That is materially heavier than the original Muff build, but still bounded
enough to test locally without extracting a full shared WDF layer first.

Expected CPU pressure:

- per sample: 2 channels × 2 clip stages × 4 Newton iterations
- per sample, outside the solver: one-pole filtering, `tanhf`, equal-power blend math
- per block: none; all state is hot and local

---

## Decision 2 — Gain Staging / Normalization

The patch stages level in four places:

1. input boost after the initial HP stage
2. stage-specific gain into each diode-pair solver
3. voice-dependent wet-path voicing after the tone stack or Tone Bypass branch
4. final equal-power dry/wet blend with a small wet makeup trim

The important design rule was that the final `softLimit(...)` should stay a safety net, not
the main tone shaper. The patch therefore spreads gain across the input, stage gains, and wet
makeup rather than pushing everything into one hard output ceiling.

---

## Decision 3 — Control Layout

The original analog pedal uses `Sustain`, `Tone`, and `Volume`. This repo still hard-wires
expression to param `2`, so the same logic from `big_muff.cpp` still applies: a literal
output control would spend the expression pedal on a set-and-forget trim. This sibling keeps
the same user surface on purpose so the nonlinear-core comparison is fair.

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Sustain` | stretched low-to-high fuzz density | `0.62f` |
| Mid | 1 | `Tone` | darker wool to sharper cut, mode-dependent | `0.48f` |
| Right / Exp | 2 | `Blend` | dry attack to mostly full fuzz | `0.74f` |

---

## Decision 4 — Parameter Taper / Mapping

### `Sustain`

- uses an eased curve rather than a literal linear pot
- reason: the hand-tuned Muff already showed that front-loaded sustain feels cramped on Endless
- target behavior: higher settings should keep adding density instead of flattening into "already maxed"

### `Tone`

- in the core voice, it still behaves like a Muff-style LP/HP balance
- in the alternate voice, it becomes a gentler bright/dark trim over a flatter mids-lift response
- reason: the middle knob should stay meaningful in both voices

### `Blend`

- uses equal-power dry/wet mixing
- keeps the right lane performance-friendly for expression
- wet makeup is intentionally modest so toe-down gets denser, not only louder

---

## Decision 4.5 — Primitive Reuse

This patch intentionally keeps its helpers local even though the repo now has enough evidence
to justify a future shared DSP layer.

Reasons:

- the WDF sibling experiment needed to land first
- the new local `WdfAntiparallelDiodePair`, `SmoothedValue`, and one-pole helpers are still
  part of the experiment surface
- extracting shared primitives before the listening comparison would blur whether the win
  came from topology, helper reuse, or both

The next logical follow-up is to compare these local helpers against a future shared
`source/dsp/` layer only after the sibling experiment has proved worth keeping.

---

## Decision 5 — Footswitch UX

- `kLeftFootSwitchPress`: bypass toggle
- `kLeftFootSwitchHold`: toggles the Tone Bypass-style mids-lift voice

The hold toggle does not clear state. Instead, the patch smooths the alternate voice with
`toneBypassMorph_` so the mode change behaves like a short morph rather than a hard branch pop.

---

## Decision 6 — LED Design

| State | Color | `Color` enum |
|---|---|---|
| Ram's Head active | Red | `Color::kRed` |
| Ram's Head bypassed | Dark red | `Color::kDarkRed` |
| Tone Bypass active | Magenta | `Color::kMagenta` |
| Tone Bypass bypassed | Dim cyan | `Color::kDimCyan` |

---

## Decision 7 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | clears stale filter and clipper state before re-entry |
| Active -> bypass | No | pass-through path is already stable |
| Tone Bypass toggle | No | smoothed morph handles the voice change without a hard reset |
| Ordinary knob move | No | sweeps should stay continuous |

---

## Implementation Notes

### Working buffer allocation

No working buffer is required. All state stays in compact per-channel members because the
experiment is about nonlinear stages, not time-domain storage.

### CPU budget estimate

Relative to `big_muff.cpp`, the heavy cost increase is the local solver:

- 16 Newton iterations per stereo sample pair
- `sinhf`, `coshf`, and `tanhf` all in the hot path

That is acceptable for an experiment, but it is exactly why this repo should not adopt a
larger shared WDF framework blindly.

### Alternate voice strategy

The alternate voice is deliberately not a second historical Muff family. The patch already
has one major experimental dimension, so the hold action stays close to the original repo
lesson: a clear Tone Bypass-style mids-lift contrast that is easy to hear during A/B tests.

---

## Testing

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
```

**Manual listening checklist:**

- [ ] `Sustain` keeps opening up beyond the early part of the sweep
- [ ] the core voice still reads as a Ram's Head-style scooped fuzz, not a generic distortion
- [ ] `Blend` remains musically useful heel-to-toe on expression
- [ ] the alternate voice is more mid-forward and flatter, not merely brighter
- [ ] the WDF sibling feels denser or more "pedal-like" than `effects/big_muff.cpp`
- [ ] bypass re-entry stays pop-free

---

## Future Improvements

1. Measure actual CPU headroom on hardware instead of inferring cost from the math shape.
2. If the sibling wins by ear, evaluate whether the local diode-pair helper belongs in a
   future shared layer or should stay patch-local.
3. If expression routing ever becomes per-patch, revisit whether a second WDF sibling with a
   literal `Volume` control is worth testing.
