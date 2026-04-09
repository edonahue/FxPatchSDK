# Tube Screamer-Inspired Overdrive — Build Walkthrough

**File:** `effects/tube_screamer.cpp`  
**Date:** 2026-04-08  
**Filter/algorithm type:** frequency-selective soft clipping with pre/post 1-pole filtering  
**Reference circuit / inspiration:** Ibanez TS808 Tube Screamer with a TS9 alternate voice  
**Primary analysis source:** <https://electrosmash.com/tube-screamer-analysis>  
**Variant context:** <https://www.analogman.com/tshist.htm>  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch builds a Tube Screamer family overdrive that keeps the classic control story:
`Drive`, `Level`, and `Tone`. The default voice targets TS808 behavior; footswitch hold toggles
a close TS9 variant. Unlike the Big Muff build, this patch keeps the original third control
instead of repurposing it, because the Tube Screamer layout already fits Endless cleanly.

---

## Circuit Reference

The ElectroSmash article describes the Tube Screamer as a frequency-selective overdrive rather
than a flat clipper. The important structure is:

1. controlled low-end trimming before clipping
2. op-amp overdrive that emphasizes the upper mids
3. post-clip tone shaping
4. output level control

The design center is the familiar mid hump around the upper mids, not maximum gain. That means
the Endless patch should sound focused, push an amp well, and stay tighter than a broader-band
distortion.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 1-pole HP, 1-pole LP, single `tanhf` clipper, post-clip tone mix |
| Working buffer needed? | No |
| State variable count | 8 floats: HP/split/tone state for two channels |
| Knob 0 (Left) | `Drive` |
| Knob 1 (Mid) | `Level` |
| Knob 2 (Right) / Exp pedal | `Tone` |
| Active LED color | `Color::kLightGreen` or `Color::kPastelGreen` |
| Bypassed LED color | `Color::kDimGreen` or `Color::kDarkLime` |
| Working buffer size needed | N/A |

---

## Decision 1 — Overdrive Topology

**Goal:** capture the Tube Screamer mid-hump feel without pretending to emulate the exact op-amp feedback network at component level.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| literal schematic emulation | closest to the analog explanation | more fragile, harder to tune, unnecessary for this repo |
| frequency-selective soft clip model | preserves the important audible behavior | less literal at component level |

**Chose:** frequency-selective soft clip model. It keeps the ElectroSmash logic intact: trim bass,
push the upper mids harder into clipping, then shape the output with a post-clip tone stage.

---

## Decision 2 — Control Layout

The original Tube Screamer control set already fits Endless, so the patch should not invent a
replacement third control.

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Drive` | edge-of-breakup to strong overdrive | `0.38f` |
| Mid | 1 | `Level` | restrained to boosted output | `0.62f` |
| Right / Exp | 2 | `Tone` | darker to brighter Tube Screamer voice | `0.48f` |

Expression on `Tone` is intentional. It keeps the classic pedal layout intact, but it means the
tone sweep must stay bounded and musical.

---

## Decision 3 — TS808 vs TS9

**Goal:** use the hold action for a family-relative alternate voice rather than an unrelated mod.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| TS9 alternate | close family jump, easy to justify from the same architecture | difference is subtle, so voicing must be disciplined |
| TS10 alternate | more contrast | drifts further from the ElectroSmash baseline |

**Chose:** TS9. The alternate mode should feel like a slightly brighter, tighter sibling, not a
different pedal class.

---

## Decision 4 — Parameter Taper and Output Control

### `Drive`

- uses an eased mapping so the overdrive rise feels progressive
- the patch avoids the MXR-style mistake of piling the main audible change at one end

### `Level`

- uses a bounded, compensated output mapping
- reason: drive and clipping density already raise perceived loudness
- goal: keep the middle knob useful at both low and high drive settings

### `Tone`

- uses a bounded post-clip tone sweep rather than an excessively wide bright/dark range
- expression heel should be dark but still usable
- expression toe should be bright and cutting without jumping into brittle fizz

---

## Decision 5 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | avoids stale filter transients |
| Active -> bypass | No | pass-through path is already stable |
| TS808 / TS9 toggle | Yes | voice coefficients and filter state shift together |
| Ordinary knob move | No | sweeps should stay continuous |

---

## Testing

```bash
bash tests/check_patches.sh
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=tube_screamer
```

**Manual listening checklist:**

- `Drive` increases grit and compression across most of the sweep
- `Level` remains practical at low and high drive
- `Tone` works clearly as both knob and expression sweep
- TS808 mode sounds smoother and slightly rounder
- TS9 mode sounds a bit tighter and brighter, but still clearly Tube Screamer family
- bypass and hold-toggle mode changes stay pop-free

---

## Future Improvements

1. Add a second, more radical alternate voice such as TS10 only if the close TS808/TS9 pair proves too subtle on hardware.
2. Revisit per-patch expression routing if later builds show that expression-on-tone is less useful than expression-on-level in practice.

---

## Related Files

- `effects/tube_screamer.cpp`
- `docs/tube-screamer-research.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
