# BBE Sonic Stomp-Inspired Enhancer — Build Walkthrough

**File:** `effects/bbe_sonic_stomp.cpp`  
**Date:** 2026-04-08  
**Filter/algorithm type:** 3-band split/recombine enhancer with band-specific delay correction  
**Reference circuit / inspiration:** BBE Sonic Stomp / Sonic Maximizer, Aion Lumin  
**Research summary:** [`docs/bbe-sonic-stomp-research.md`](bbe-sonic-stomp-research.md)  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch adapts the public BBE Sonic Stomp / Sonic Maximizer concept to the Endless as a
guitar-oriented enhancer. Instead of literal chip emulation, it uses a broad low/mid/high
split, applies band-specific timing corrections, and adds those corrections back to a common
dry path. The result is meant to feel tighter and clearer rather than obviously filtered.

The core patch uses a Lumin-style 3-knob layout: `Contour`, `Process`, and `Midrange`.
Because the current repo routes expression to the Right knob globally, expression explicitly
controls `Midrange` when plugged in. A subtle stereo doubler is available on footswitch hold
as a stretch goal, but it stays off by default.

---

## Circuit Reference

### Stock Sonic Stomp / Sonic Maximizer

The official Sonic Stomp exposes two controls:

- `Contour` for low-frequency phase-corrected emphasis
- `Process` for high-frequency phase correction

Public manual materials anchor those controls around `50 Hz` and `10 kHz`, respectively.

### Aion Lumin interpretation

The Aion `Lumin` documentation provides the clearest public clone-level explanation:

- the process can be understood as a three-part state-variable split
- low, mid, and high regions are treated independently
- the stock mid contribution is fixed, but can be exposed as a third `Midrange` control
- guitar users often prefer different crossover points than the stock `50 Hz` / `10 kHz`

### Digital adaptation chosen here

For Endless, the implementation uses:

- a dry/program path
- a complementary low/mid/high split
- band-specific delays around a common base delay
- correction deltas added back to the dry path

This follows the public behavioral consensus while staying simple, stable, and SDK-safe.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 1-pole LP split + band-specific delay correction |
| Working buffer needed? | Yes, but only for the optional doubler |
| State variable count | 2 LP states + ring buffers for dry/low/mid/high + optional doubler state |
| Knob 0 (Left) | Contour |
| Knob 1 (Mid) | Process |
| Knob 2 (Right) / Exp pedal | Midrange |
| Active LED color | `Color::kLightGreen` or `Color::kLightBlueColor` |
| Bypassed LED color | `Color::kDimGreen` or `Color::kDimBlue` |
| Working buffer size needed | `2 * 1536` floats for the doubler |

---

## Decision 1 — Core Enhancer Topology

**Goal:** capture the public BBE/Lumin behavior with a DSP structure that is lightweight and
explainable.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| Literal chip/schematic emulation | Highest historical fidelity | Proprietary chip, incomplete public data, unnecessary complexity |
| Static multi-band EQ | Easy to implement | Misses the time/phase-correction character |
| Split + band-specific correction + dry recombine | Matches public descriptions, simple, controllable | Approximation, not literal analog emulation |

**Chose: split + correction + recombine.** This is the best match to the public consensus:
the signal is split into broad bands, those bands are treated differently, and correction is
mixed back with a dry/program path.

---

## Decision 2 — Guitar-Oriented Tuning

**Problem:** the stock `50 Hz` / `10 kHz` anchors are broad, and public clone builders often
retune them for guitar.

**Chosen fixed tuning for v1:**

- low crossover: `~80 Hz`
- high crossover: `~5.6 kHz`

**Why:** this keeps `Contour` near the low-E / cabinet-thump region and moves `Process`
closer to pick articulation and usable upper harmonics for electric guitar.

This patch does not expose crossover switches in v1. The fixed tuning is the default
guitar-oriented choice.

---

## Decision 3 — Control Layout

The core design prioritizes the Lumin-style 3-knob layout over expression support:

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | Contour | low-band correction amount | 0.45 |
| Mid | 1 | Process | high-band correction amount | 0.45 |
| Right / Exp | 2 | Midrange | bandpass correction amount | 0.5 |

**Constraint:** this repo currently routes expression to `param 2` globally. That means
expression intentionally drives `Midrange`, not an independent overall intensity control.

**Reasoning:** `Midrange` is a musically useful live target, and this preserves the
`Contour + Process + Midrange` layout with no SDK changes.

---

## Decision 4 — Phase / Timing Model

The public literature describes high and low corrections in phase terms. In the digital
implementation, the patch approximates this with **relative band delays** around a shared
dry path.

**Shared base delay:** `32 samples`

**Band delay behavior:**

- `Contour` increases low-band lag relative to the dry path
- `Process` reduces high-band delay relative to the dry path
- `Midrange` adjusts the bandpass correction around the shared base delay

The output formula is:

```cpp
output = dryBase
       + contourBlend * (lowShift  - lowBase)
       + midBlend     * (midShift  - midBase)
       + processBlend * (highShift - highBase);
```

This preserves a dry anchor while adding only the correction deltas.

---

## Decision 5 — Footswitch UX

The core patch uses the available Endless actions as follows:

- `kLeftFootSwitchPress` → bypass toggle
- `kLeftFootSwitchHold` → toggle optional doubler mode

The doubler is a stretch goal and is intentionally conservative:

- off by default
- subtle stereo widening/thickening only
- no extra knob assignment in v1

This keeps the core enhancer simple while still making room for the requested exploration.

---

## Decision 6 — LED Design

| State | Color | `Color` enum |
|---|---|---|
| Enhancer only, active | Light green | `Color::kLightGreen` |
| Enhancer + doubler, active | Light blue | `Color::kLightBlueColor` |
| Enhancer only, bypassed | Dim green | `Color::kDimGreen` |
| Enhancer + doubler, bypassed | Dim blue | `Color::kDimBlue` |

The dim color preserves mode visibility while bypassed.

---

## Decision 7 — Doubler Design

**Goal:** add a small Mimiq-style stretch mode without turning the patch into a delay pedal.

**Chosen approach:**

- short decorrelated delays per channel
- independent slow modulation on each side
- light low-pass smoothing on the delayed voices
- low blend levels so the effect reads as width/thickness, not slapback

This is not a literal Mimiq emulation. It is a practical Endless-friendly nod to the same
idea: overdub-like widening through slight timing variation.

---

## Decision 8 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass → active | Yes | avoid stale filter/delay state pops |
| Active → bypass | No | dry passthrough, no state readout needed |
| Doubler toggle | Yes (doubler state) | avoid stale echoes/smear |
| Parameter moves | No | continuous correction is part of the effect feel |

---

## Implementation Notes

### Band split

The split uses two low-pass stages:

- `low = LP(low_fc)`
- `high = input - LP(high_fc)`
- `mid = LP(high_fc) - LP(low_fc)`

This guarantees:

`low + mid + high = input`

which keeps the correction math predictable.

### Memory

The enhancer core uses only small fixed-size ring buffers in object state.

The optional doubler uses a small partition of the working buffer:

- `1536` floats for left
- `1536` floats for right

### CPU budget

Core enhancer cost is low:

- two 1-pole LP updates per channel
- a handful of ring-buffer reads
- a few multiplies/adds for correction recombination

The doubler adds:

- two interpolated delay taps
- two `sinf()` calls per sample pair
- two one-pole smoothing filters

This is still well within the Endless CPU budget for a single patch.

---

## Testing

```bash
# From the repo root:
bash tests/check_patches.sh
```

**Manual listening checklist:**
- [ ] Default settings sound subtly clearer and tighter than bypass
- [ ] `Contour` increases low-end firmness/body without flub
- [ ] `Process` increases clarity/presence without harsh fizz
- [ ] `Midrange` moves from leaner to fuller guitar body in a useful range
- [ ] With expression connected, heel-down gives less mid correction and toe-down gives more
- [ ] With expression connected, `Contour` and `Process` still respond normally on the knobs
- [ ] With expression connected, the physical Right knob is ignored as expected
- [ ] Extreme settings do not clip or behave erratically
- [ ] Bypass engages/disengages cleanly
- [ ] Hold toggles doubler mode cleanly
- [ ] Doubler mode widens/thickens without obvious slapback

---

## Future Improvements

1. Add per-patch expression routing support if expression should later target overall enhancer
   intensity instead of inheriting the global `param 2` mapping.
2. Add crossover/voicing modes to expose stock-vs-guitar frequency options from the public
   clone ecosystem.
3. Replace the fixed doubler behavior with a controllable `Tightness` or `Amount` mode if
   the SDK control surface is extended.

---

## Related Files

- `effects/bbe_sonic_stomp.cpp` — the patch implementation
- `docs/bbe-sonic-stomp-research.md` — source-backed circuit and clone research summary
- `docs/circuit-to-patch-conversion.md` — general analog-to-DSP methodology
- `docs/endless-reference.md` — SDK constraints and expression routing behavior
- `internal/PatchCppWrapper.cpp` — current global expression pedal routing
