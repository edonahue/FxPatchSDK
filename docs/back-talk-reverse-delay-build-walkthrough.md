# Back Talk Reverse Delay — Build Walkthrough

**File:** `effects/back_talk_reverse_delay.cpp`  
**Date:** 2026-04-08  
**Filter/algorithm type:** chunk-based reverse delay using the working buffer  
**Reference circuit / inspiration:** Danelectro Back Talk Reverse Delay  
**Primary analysis source:** <https://electrosmash.com/back-talk-analysis>  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

The Danelectro Back Talk is one of the cleanest ElectroSmash candidates for an Endless patch
because it is already a pure digital reverse delay with a 3-knob control story: `Mix`,
`Speed`, and `Repetitions`. This implementation keeps that core idea, then adds a modest
Endless-native extension: a hold-toggle `Texture` mode that layers an older reverse slice
under the main slice for a smoother reverse bloom.

Expression is routed to `Mix` because this repo hardcodes expression to param `2`, and
`Mix` is the most immediately useful live-performance target. Tap tempo is not a core v1
feature, but the timing model is deliberately built around chunk duration so a future tap
source can drive the same time parameter cleanly.

---

## Circuit Reference

ElectroSmash frames the Back Talk as a pure digital reverse delay built around:

- an analog front end and output buffer
- an audio codec running at `22.05 kHz`
- microcontroller-managed RAM storage
- reverse chunking and delayed playback
- 3 user controls: `Mix`, `Speed`, `Repetitions`

The key behavioral idea is simple and important:

1. capture incoming audio into memory
2. chop it into delay windows
3. play those windows backward after a short delay
4. blend them with the dry signal
5. feed a controlled percentage back for repeating reverse echoes

ElectroSmash also notes the original memory budget is about `1.45 seconds` total at
`22.05 kHz`. Endless has far more working memory and runs at `48 kHz`, so this patch
preserves the chunked reverse-delay behavior rather than copying the old hardware limits.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | circular delay buffer + reverse chunk reader + feedback path |
| Working buffer needed? | Yes |
| State variable count | moderate: write head, chunk playback state, feedback state, mode flags |
| Knob 0 (Left) | Speed |
| Knob 1 (Mid) | Repetitions |
| Knob 2 (Right) / Exp pedal | Mix |
| Active LED color | `Color::kLightBlueColor` / `Color::kMagenta` |
| Bypassed LED color | `Color::kDimBlue` / `Color::kDimCyan` |
| Working buffer size needed | `2 * 131072` floats |

---

## Decision 1 — Reverse Chunk Topology

**Goal:** preserve the Back Talk's core sound without overcomplicating the first implementation.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| granular reverse cloud | very flexible, modern texture potential | no longer feels like a pedal-style reverse delay |
| full continuously reversing buffer | conceptually simple | hard to make musically legible; weak pedal feel |
| fixed reverse chunks with feedback | closest to ElectroSmash behavior; easy to reason about | chunk edges need careful smoothing |

**Chose: fixed reverse chunks with feedback.** This matches the ElectroSmash explanation,
keeps the control story clear, and gives a pedal-like result quickly.

---

## Decision 2 — Gain Staging / Normalization

The biggest risks in a reverse delay are:

- harsh chunk-edge clicks
- wet-path loudness spikes at high mix
- feedback building too fast at high repetitions

The implementation handles this with:

- a short fade envelope on every reverse chunk
- equal-power dry/wet mixing
- a bounded feedback gain curve
- a light one-pole low-pass in the feedback path to keep repeats musical

That keeps the output within `(-1, +1)` without making the patch feel timid.

---

## Decision 3 — Control Layout

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | Speed | short reverse stab -> long reverse phrase | `0.42` |
| Mid | 1 | Repetitions | single reverse hit -> near-runaway repeats | `0.32` |
| Right / Exp | 2 | Mix | dry-led -> wet-led reverse blend | `0.38` |

This stays close to the original Back Talk layout, which is one reason the pedal is such a
good Endless target.

Expression is intentionally mapped to `Mix`, not `Speed` or `Repetitions`, because:

- this repo routes expression to param `2`
- `Mix` is the safest live target
- heel/toe dry-wet movement is musically obvious on a reverse delay

---

## Decision 4 — Parameter Taper / Mapping

### Speed

`Speed` controls reverse chunk duration, which is effectively both delay time and phrase size.
This is a wide time range, so it should not be linear.

Chosen behavior:

- log-style time mapping
- short settings remain useful for rhythmic reverse stabs
- long settings open up phrase-like reversals

The practical target range is approximately `75 ms` to `1.2 s`.

### Repetitions

`Repetitions` uses a bounded power-law mapping so most of the knob stays in controllable
repeat territory and only the top end approaches self-oscillation.

Checks that matter:

- low settings should still produce one obvious reverse repeat
- middle settings should create musical decays
- the top end should feel dramatic, not instantly broken

### Mix

`Mix` uses an equal-power dry/wet crossfade so the expression pedal remains useful and the
wet end does not feel like a crude volume jump.

Checks that matter:

- heel-down should preserve dry articulation
- toe-down should make the reverse voice dominant without a harsh loudness spike
- most of the pedal travel should feel musically useful

---

## Decision 5 — Footswitch UX

The current SDK only exposes one footswitch with press and hold actions.

Chosen mapping:

- `kLeftFootSwitchPress` -> bypass toggle
- `kLeftFootSwitchHold` -> `Texture` mode toggle

`Texture` mode is the Endless-native addition for v1. Instead of only one reverse chunk, it
blends in an older half-offset reverse slice at lower level. This makes the effect feel
smoother and more cinematic without abandoning the core Back Talk identity.

### Tap tempo

Tap tempo is a stretch goal, not a v1 feature.

The design is intentionally "tap-ready" because `Speed` already maps to chunk length, but a
real tap-tempo UX would require resolving the footswitch-event conflict with `Texture` mode.
That should be handled in a later pass, not forced into the first implementation.

---

## Decision 6 — LED Design

| State | Color | `Color` enum |
|---|---|---|
| Normal mode, active | Light blue | `Color::kLightBlueColor` |
| Normal mode, bypassed | Dim blue | `Color::kDimBlue` |
| Texture mode, active | Magenta | `Color::kMagenta` |
| Texture mode, bypassed | Dim cyan | `Color::kDimCyan` |

This keeps the mode visible even while bypassed.

---

## Decision 7 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass -> active | Yes | avoid stale reverse fragments on re-engage |
| Active -> bypass | No | pass-through only |
| Texture mode toggle | playback state yes, full buffer no | avoid abrupt chunk-state transitions without throwing away all recent timing context |
| Knob / expression moves | No | continuity matters more than reset |

---

## Implementation Notes

### Working buffer allocation

The patch uses two large circular buffers in the working buffer:

- left: `131072` samples
- right: `131072` samples

This is intentionally larger than the maximum reverse chunk length so the playback window is
never overwritten while it is still being read.

### CPU budget estimate

Per sample, per channel:

- one circular-buffer write
- one reverse-buffer read
- optional second reverse read in texture mode
- a few multiplies/adds for envelope, mix, and feedback filtering

This is comfortably within the Endless budget and substantially cheaper than more elaborate
granular or spectral reverse approaches.

### Key gotchas

- chunk edges must be windowed or the effect will click badly
- maximum chunk size must stay below half the ring-buffer length
- feedback must be bounded and slightly damped or the top of `Repetitions` becomes unusable
- `Speed` changes should apply on the next chunk boundary, not force-reset the currently playing chunk

---

## Testing

```bash
# From the repo root:
bash tests/check_patches.sh
```

**Manual listening checklist:**

- [ ] `Speed` low: tight reverse stabs, no obvious clicks
- [ ] `Speed` high: phrase-length reversals, still musically legible
- [ ] `Repetitions` low/mid/high all do useful work
- [ ] `Mix` and expression move smoothly from dry-led to wet-led
- [ ] bypass engages and disengages cleanly
- [ ] normal mode feels tighter than texture mode
- [ ] texture mode sounds smoother/denser, not simply louder

---

## Future Improvements

1. Add a real tap-tempo mode or tap-to-quantize speed system after deciding how to resolve the footswitch conflict.
2. Add optional feedback tone or diffusion control if the base reverse engine proves solid on hardware.

---

## Related Files

- `effects/back_talk_reverse_delay.cpp`
- `docs/electrosmash-pedal-shortlist.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
- `internal/PatchCppWrapper.cpp`
