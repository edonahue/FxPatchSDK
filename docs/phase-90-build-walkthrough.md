# Phase 90-Inspired Phaser — Build Walkthrough

**File:** `effects/phase_90.cpp`  
**Date:** 2026-04-08  
**Filter/algorithm type:** four cascaded first-order all-pass stages with shared LFO and optional feedback  
**Reference circuit / inspiration:** MXR Phase 90 block-logo voice with script-mod vintage alternate  
**Primary analysis source:** <https://www.electrosmash.com/mxr-phase90>  
**Script-mod context:** <https://www.analogman.com/mxr.htm>  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch aims to feel like a real Phase 90 first and an Endless patch second. It keeps a single
public control for `Speed`, uses the hold action to switch between block and script voices, and
resists the temptation to expose extra mix or feedback knobs. The only concession to the Endless
surface is that expression mirrors `Speed`.

---

## Circuit Reference

ElectroSmash describes the Phase 90 as a four-stage phaser with:

1. input buffer
2. four variable phase-shifting stages
3. LFO
4. dry/wet recombination
5. optional feedback emphasis in later block-logo versions

The script-mod distinction is unusually direct: removing the feedback emphasis makes the effect
smoother and less chewy. That makes the block/script toggle the most authentic use of the hold
action in this repo.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 4 first-order all-pass stages, triangle LFO, dry/wet mix |
| Working buffer needed? | No |
| State variable count | 10 floats: 4 all-pass states per channel plus feedback history |
| Knob 0 (Left) | unused |
| Knob 1 (Mid) | `Speed` |
| Knob 2 (Right) / Exp pedal | `Speed` mirror for expression |
| Active LED color | `Color::kBlue` or `Color::kBeige` |
| Bypassed LED color | `Color::kDimBlue` or `Color::kDimWhite` |
| Working buffer size needed | N/A |

---

## Decision 1 — Phaser Topology

**Goal:** keep the sound recognizably Phase 90 instead of building a generic phaser.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| generic SVF/notch phaser | easy to prototype | does not match the classic four-stage sweep feel |
| cascaded first-order all-pass phaser | closer to the original pedal behavior | requires more internal state and careful coefficient handling |

**Chose:** cascaded all-pass topology. This is the right level of authenticity for the pedal family.

---

## Decision 2 — Control Layout

The whole point of this build is to keep the original one-knob story.

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | unused | N/A | `0.0f` |
| Mid | 1 | `Speed` | slow sweep to fast sweep | `0.34f` |
| Right / Exp | 2 | `Speed` mirror | same as `Speed` | `0.34f` |

The Right knob is not a second public control. It is only the firmware lane that expression must
use in this repo.

---

## Decision 3 — Block vs Script

**Goal:** make the alternate mode authentic rather than decorative.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| deeper / modern mode | obvious contrast | not faithful to the original pedal family |
| script-mod mode | historically grounded and musically useful | subtler, so the voicing difference must be disciplined |

**Chose:** script-mod mode. The alternate voice mainly changes feedback intensity and overall sweep feel.

---

## Decision 4 — Speed and Minimal-UI Discipline

### `Speed`

- uses a log-style rate mapping
- reason: the ear hears sweep-rate changes more naturally on a ratio scale than on a linear scale
- default is moderate, not ultra-slow

### Unused controls

- Left is intentionally ignored
- Right is not documented as a public knob; it only mirrors speed because of expression routing
- reason: this patch should preserve the one-knob Phase 90 identity instead of filling the UI just because it can

---

## Decision 5 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | avoids stale all-pass and feedback transients |
| Active -> bypass | No | pass-through is already stable |
| Block / script toggle | Yes | feedback path and voicing change together |
| Ordinary speed move | No | preserve sweep continuity |

---

## Testing

```bash
bash tests/check_patches.sh
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=phase_90
```

**Manual listening checklist:**

- center knob produces a useful slow-to-fast sweep across most of the travel
- expression feels like a natural mirror of Speed
- block mode sounds stronger and chewier
- script mode sounds smoother and more vintage-balanced
- both modes stay near unity and do not feel like volume shifts
- bypass and hold-toggle are pop-free

---

## Future Improvements

1. Add per-patch expression routing if the repo later wants expression without exposing the Right knob lane at all.
2. Explore a two-stage Phase 45 family patch separately rather than overloading this one with extra voicing modes.

---

## Related Files

- `effects/phase_90.cpp`
- `docs/phase-90-research.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
