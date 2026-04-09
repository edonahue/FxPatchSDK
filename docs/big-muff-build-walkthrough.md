# Big Muff Pi-Inspired Fuzz — Build Walkthrough

**File:** `effects/big_muff.cpp`  
**Date:** 2026-04-08  
**Filter/algorithm type:** cascaded soft clipping with passive-tone-stack-inspired LP/HP blend  
**Reference circuit / inspiration:** Electro-Harmonix Big Muff Pi, voiced toward the Ram's Head family  
**Primary analysis source:** <https://www.electrosmash.com/big-muff-pi-analysis>  
**Variant context:** <https://www.kitrae.net/music/big_muff_historyB.html>  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch translates the Big Muff into an Endless-friendly fuzz/sustain effect. The core mode
keeps the classic stacked clipping stages and the famous scooped tone blend, then a footswitch
hold toggles a `Tone Bypass`-inspired voice with more mids and a louder, flatter feel. The Right
knob becomes `Blend` so expression can do something performance-oriented instead of owning a
static output-volume control.

---

## Circuit Reference

ElectroSmash breaks the Big Muff into four blocks:

1. input booster
2. two cascaded clipping stages
3. passive tone control
4. output booster

The two clipping stages matter more than any one exact component value. The circuit gets its
signature from repeated gain-plus-clipping passes that are also bandwidth-limited, which smooths
the fuzz and increases sustain. ElectroSmash also shows the passive tone stack as a low-pass /
high-pass blend with a strong scoop around 1 kHz near the midpoint.

This build keeps that logic, then makes two Endless-specific adaptations:

- use `Blend` instead of `Volume` on param `2`
- use footswitch hold for a Tone Bypass-style mids-lift alternate voice

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 1-pole HP, 1-pole LP, cascaded `tanhf` clipping, equal-power blend |
| Working buffer needed? | No |
| State variable count | 18 floats: HP/LP/filter state for two channels |
| Knob 0 (Left) | `Sustain` |
| Knob 1 (Mid) | `Tone` |
| Knob 2 (Right) / Exp pedal | `Blend` |
| Active LED color | `Color::kRed` or `Color::kMagenta` |
| Bypassed LED color | `Color::kDarkRed` or `Color::kDimCyan` |
| Working buffer size needed | N/A |

---

## Decision 1 — Core Topology

**Goal:** preserve the Big Muff identity without implementing transistor-level bias simulation.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| literal transistor-stage emulation | closer to schematic language | more fragile, more tuning-heavy, harder to justify in stock SDK |
| cascaded gain/clip/filter model | captures the audible structure cleanly | less literal at the component level |

**Chose:** cascaded gain/clip/filter model. It preserves the important architecture from the
ElectroSmash article: input conditioning, two clipping stages, tone shaping, and recovery.

---

## Decision 2 — Control Layout

The analog pedal uses `Sustain`, `Tone`, and `Volume`, but this fork hardcodes expression to
param `2`. A literal `Volume` mapping would waste the expression pedal on a set-and-forget trim.

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Sustain` | low fuzz to near-saturated sustain | `0.56f` |
| Mid | 1 | `Tone` | bass-heavy to bright, mode-dependent | `0.48f` |
| Right / Exp | 2 | `Blend` | dry to mostly fuzz | `0.74f` |

This keeps the two defining Muff controls and makes the third control performance-useful.

---

## Decision 3 — Tone Bypass Alternate Voice

**Goal:** use the hold action for a second voice that is obviously different and useful.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| Triangle-style alternate | historically close to Ram's Head | difference can read as subtle in a first-pass digital build |
| Green Russian alternate | strong family contrast | asks the patch to solve several voicing shifts at once |
| Tone Bypass-inspired alternate | very clear contrast, easy to expose on one hold action | less historically specific than naming a full variant |

**Chose:** Tone Bypass-inspired alternate. Normal mode keeps the classic scooped Muff tone stack.
Hold mode removes that scoop and uses a flatter post-clip voicing so the alternate mode sounds
more forward and louder.

---

## Decision 4 — Parameter Taper and Loudness

### `Sustain`

- uses an eased power-law mapping instead of raw linear pot math
- reason: literal linear mapping tends to bunch the most dramatic change too late in the sweep
- acceptance check: the knob should add fuzz and compression across most of the travel

### `Tone`

- normal mode uses a classic LP/HP blend
- tone-bypass mode keeps the same knob alive as a gentler top-end filter
- reason: disabling the middle knob in the alternate mode would waste the control surface

### `Blend`

- uses equal-power dry/wet blending
- wet level is slightly trimmed to avoid a loudness cliff near full fuzz
- expression still makes sense because the sweep moves from defined pick attack to mostly full Muff

---

## Decision 5 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | avoids pops from stale filter state |
| Active -> bypass | No | pass-through path does not need extra work |
| Tone mode toggle | Yes | the two voicings use different filter/state assumptions |
| Ordinary knob move | No | preserves continuity during live sweeps |

---

## Testing

```bash
bash tests/check_patches.sh
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=big_muff
```

**Manual listening checklist:**

- `Sustain` adds fuzz and compression across most of the sweep
- `Tone` in Ram's Head mode moves clearly from heavy/round to cutting/bright
- midpoint `Tone` in Ram's Head mode still sounds intentionally scooped
- `Blend` keeps heel-down articulate and toe-down mostly full fuzz without a sharp volume jump
- hold toggle creates a clearly more mid-forward and louder alternate voice
- bypass and mode changes do not pop
- expression on `Blend` feels smooth and useful

---

## Future Improvements

1. Add a true output-level control if the repo eventually supports per-patch expression routing.
2. Explore a second historical Muff family mode such as Green Russian once the core Ram's Head build is proven on hardware.

---

## Related Files

- `effects/big_muff.cpp`
- `docs/big-muff-research.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
