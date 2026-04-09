# Klon Centaur-Inspired Overdrive — Build Walkthrough

**File:** `effects/klon_centaur.cpp`  
**Date:** 2026-04-08  
**Filter/algorithm type:** clean/dirty summing overdrive with active treble shelving  
**Reference circuit / inspiration:** Klon Centaur with a Tone Mod alternate voice  
**Primary analysis source:** <https://www.electrosmash.com/klon-centaur-analysis>  
**Mod context:** <https://www.coda-effects.com/p/klon-centaur-mods-and-tweaks.html>  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch targets the Klon Centaur as a boost-to-overdrive effect rather than a general-purpose
distortion. The core design keeps the clean/dirty interaction that defines the pedal, uses an
active treble shelf instead of a generic tone filter, and maps expression to `Output` so the patch
remains useful as a live boost. Footswitch hold toggles a fuller Tone Mod-style alternate voice.

---

## Circuit Reference

ElectroSmash breaks the Klon into:

1. input buffer
2. op-amp gain stage with germanium clipping
3. summing stage
4. active tone control
5. output stage

The important behavior is not just the clipping stage. The Klon keeps cleaner signal content alive
while the clipped branch rises, then shapes the result with an active treble shelf. That interaction
is the main thing to preserve in a stock-SDK patch.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 1-pole HP, 1-pole LP, `tanhf` clipping, active shelf-style branch mix |
| Working buffer needed? | No |
| State variable count | 10 floats: clean LP, clip HP, clip LP, shelf LP for two channels |
| Knob 0 (Left) | `Gain` |
| Knob 1 (Mid) | `Treble` |
| Knob 2 (Right) / Exp pedal | `Output` |
| Active LED color | `Color::kLightYellow` or `Color::kBeige` |
| Bypassed LED color | `Color::kDimYellow` or `Color::kDimWhite` |
| Working buffer size needed | N/A |

---

## Decision 1 — Core Topology

**Goal:** preserve the Klon’s transparent overdrive feel without pretending to reproduce the full power-supply design.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| single-path overdrive | simpler to code | loses the clean/dirty interaction that makes the Klon special |
| clean/dirty summing model | preserves the defining behavior | requires more careful gain balancing |

**Chose:** clean/dirty summing model. The clipping stage alone does not explain the Klon sound well enough.

---

## Decision 2 — Control Layout

The original control set already fits Endless well, and the output control is the best expression target for this pedal family.

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Gain` | clean push to smooth overdrive | `0.28f` |
| Mid | 1 | `Treble` | active cut/boost shelf | `0.52f` |
| Right / Exp | 2 | `Output` | unity-ish to strong boost | `0.66f` |

This keeps the pedal identity intact while making expression performance-friendly.

---

## Decision 3 — Tone Mod Alternate Voice

**Goal:** add one alternate Klon-family voice without drifting into a different pedal.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| no alternate mode | simpler, more purist | leaves the hold action unused |
| more gain mode | obvious contrast | pushes the patch away from the classic Centaur lane |
| Tone Mod | useful family variation, fuller response | subtler than a drastic gain switch |

**Chose:** Tone Mod. The alternate mode lowers the shelving corner and keeps the response fuller, closer to the common C14-style Klon mod.

---

## Decision 4 — Gain, Treble, and Output Behavior

### `Gain`

- uses an eased mapping
- also rebalances clean and clipped branch levels internally
- reason: the Klon gain control is not just “more distortion”

### `Treble`

- implemented as an active shelf-style split rather than a generic low-pass
- midpoint is intended to be musically neutral
- extremes should be useful, not only “dull” or “ice-pick”

### `Output`

- uses a compensated taper for live boost use
- expression should feel like a practical heel-to-toe boost range, not a sudden jump at the top

---

## Decision 5 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | avoids stale filter transients |
| Active -> bypass | No | pass-through path does not need clearing |
| Stock / Tone Mod toggle | Yes | shelf and clipping voicing change together |
| Ordinary knob move | No | keep parameter sweeps continuous |

---

## Testing

```bash
bash tests/check_patches.sh
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=klon_centaur
```

**Manual listening checklist:**

- `Gain` moves from clean push to smooth overdrive across most of the sweep
- lower gain settings still keep attack and openness
- `Treble` behaves like a useful active shelf, not a narrow bright switch
- `Output` stays controllable at higher gain and feels natural under expression
- stock mode sounds open and stackable
- Tone Mod sounds fuller and less thin, but still clearly Klon-family
- bypass and hold-toggle transitions stay pop-free

---

## Future Improvements

1. Revisit per-patch expression routing if later testing shows that expression-on-output is less valuable than expected for this effect.
2. Explore a KTR-style or split-gain alternate mode only if the stock/Tone Mod pair proves too subtle on hardware.

---

## Related Files

- `effects/klon_centaur.cpp`
- `docs/klon-centaur-research.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
