# Harmonica (Blues Bullet-Mic) — Build Walkthrough

**File:** `effects/harmonica.cpp`
**Date:** 2026-04-16
**Filter/algorithm type:** dual Chamberlin bandpass formants + asymmetric tanh reed saturation with body/edge split + post-LPF + AM tremolo + fractional-delay micro-chorus
**Reference voice / inspiration:** Shure Green Bullet cupped into a cranked tube amp — Chicago blues harmonica (Little Walter, Sonny Boy Williamson II)
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

This patch is a timbral transformation, not a pitch transformation. The SDK exposes
no pitch-shifter, formant-shifter, or harmonizer block, so a guitar played through
this patch still produces guitar pitch content — what changes is the spectral
shape, compression, and attack character. Played with appropriate technique
(neck pickup, tone rolled to ~3, single-note lines above the 5th fret,
fingerstyle, no open low strings), the illusion lands well enough to be musically
useful. Chords and open low strings break the illusion immediately; that
trade-off is accepted and documented in-patch.

---

## Circuit / Voice Reference

A cupped blues harmonica rig has four linked elements:

1. **Hand-cup Helmholtz resonator** — palm aperture sweeps a resonant formant
   across a range roughly 300–2500 Hz. This is the single most identifiable cue.
2. **Second "nasal" formant** near 1.5–1.8 kHz from the reed-plate cavity, giving
   the two-peak vowel structure that separates a harp from a cocked wah.
3. **Bullet-mic + tube-amp breakup** — a high-impedance Green Bullet feeding a
   cranked small amp produces asymmetric reed-like saturation with a hard
   rolloff above ~2.5 kHz.
4. **Dual-reed beating** — a gentle sub-audible wobble from two slightly detuned
   reeds per hole, plus hand-motion air-column modulation.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | 2× Chamberlin SVF BP, 1-pole HP/LP, body/edge LP split + asymmetric tanh, AM tremolo, fractional-delay chorus, DC blocker, soft-limit |
| Working buffer needed? | Yes — 960 floats for stereo micro-chorus delay lines |
| State variable count | 22 per-channel scalars + 3 LFO phases + 1 write index + mode/bypass flags |
| Knob 0 (Left) | `Tone / Cup` trim |
| Knob 1 (Mid) | `Reed / Drive` (saturation) |
| Knob 2 (Right) / Exp pedal | `Waa / Cup sweep` (hand-cup formant sweep) |
| Active LED color | `Color::kLightYellow` (Cupped) or `Color::kLightBlueColor` (Open) |
| Bypassed LED color | `Color::kDimYellow` (Cupped) or `Color::kDimBlue` (Open) |
| Working buffer size needed | 2 × 480 samples = 960 floats |

---

## Decision 1 — No Pitch Shift, So Commit to Timbre

**Goal:** produce a convincing harmonica voice under the SDK's real constraints.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| Try to simulate pitch transposition via granular/ARP tricks | Could move guitar pitches into harp range | SDK C++ API has none of those primitives; would require compiled Playground artifacts |
| Accept the guitar's pitch content and transform only timbre | Fits the SDK cleanly; produces a performable patch today | Chords and open low strings will expose the trick |

**Chose:** timbre-only transformation. Documented the playing-style constraint
in the patch header so future users aren't surprised when a G chord at the
first fret sounds like a bandpass-filtered guitar.

---

## Decision 2 — Two Formants, Not One

**Goal:** differentiate this patch from "a wah stuck in one position."

A wah is a single swept peak. A cupped harp has a swept cup formant *and* a
roughly fixed nasal peak from the reed-plate cavity. Combining a log-swept
Chamberlin BP (cup) with a fixed Chamberlin BP at ~1.7 kHz (nasal) produces
the two-peak vowel structure that reads as "harp" rather than "filtered guitar."

---

## Decision 3 — Control Layout

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Tone / Cup` | bright and loose to dark and tight | `0.55f` |
| Mid | 1 | `Reed / Drive` | light reed to cranked bullet-amp breakup | `0.48f` |
| Right / Exp | 2 | `Waa / Cup sweep` | log-swept formant 320 → 2000/2500 Hz | `0.45f` |

The expression pedal controls param 2 automatically via the existing
`internal/PatchCppWrapper.cpp` routing — the hand-cup sweep is the signature
harmonica gesture and maps naturally to heel-to-toe pedal motion.

---

## Decision 4 — Asymmetric Reed Saturation

Real harmonica reeds have different blow/draw breakup curves; modeling that
asymmetry gives the characteristic "chirp" on attacks.

**Implementation:** split the signal at ~700 Hz into a `body` (low-pass) and
`edge` (residual) branch — the same pattern used by `effects/tube_screamer.cpp`
to keep low notes from turning to mud. Scale each branch separately and feed
into an asymmetric tanh where the negative swing clips harder by a voicing-
dependent factor (1.10× for Open, 1.30× for Cupped). The asymmetric clipper
produces DC, so a 25 Hz high-pass sits before the soft-limit to scrub it.

---

## Decision 5 — Open vs Cupped Hold Toggle

**Goal:** give the player two distinct voices without adding more knobs.

The hold toggle switches between two voicing presets that move eleven parameters
together (Q, formant peak gain, formant-2 mix, pre-HPF, low-shelf cut, post-LPF,
clip gain, asymmetry, tremolo depth, chorus wet, WAA toe position). The `Tone`
knob then applies a trim on top so each voicing still has useful travel.

| Parameter | Open | Cupped |
|---|---|---|
| Formant-1 Q | 3.0 | 6.0 |
| Formant-1 peak gain | ×3.0 (+9.5 dB) | ×4.2 (+12.5 dB) |
| Formant-2 mix | 0.35 | 0.50 |
| Pre-HP fc | 120 Hz | 160 Hz |
| Low-shelf cut | −2 dB | −5 dB |
| Saturation clipGain | 2.2 | 3.4 |
| Asymmetry | 1.10 | 1.30 |
| Post-LP fc | 2.8 kHz | 1.9 kHz |
| Tremolo depth | ±0.8 dB | ±1.4 dB |
| µ-chorus wet | 0.08 | 0.16 |
| WAA fc_max | 2500 Hz | 2000 Hz |

Both the press-to-bypass toggle and the hold-to-switch-voicing path call
`clearState()`, which zeros all filter state and the delay lines so mode
changes and bypass re-engages are pop-free.

---

## Decision 6 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass → active | Yes | avoids stale SVF/clipper/DC-blocker transients |
| Active → bypass | No | pass-through is already stable |
| Open / Cupped toggle | Yes | Q and fc change together; stale state would cause a pop |
| Ordinary knob move | No | preserve sweep continuity under expression-pedal motion |

A subtle gotcha: `init()` intentionally does **not** touch `delayL_` or
`delayR_`. The host calls `setWorkingBuffer` before `init()`, so clobbering the
pointers in `init()` would leave `processAudio` with a dead delay line. This
matches the pattern used by `effects/chorus.cpp`.

---

## Testing

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
bash scripts/build_effects.sh --effect harmonica
```

**Manual listening checklist:**

- Clean DI, neck pickup, tone=3, fingerstyle bends at the 3rd-fret blues box.
- Heel → toe expression sweep reads as "wah → open harp" morph, not a synthy filter.
- Hold-toggle: Open is airier with louder highs; Cupped is dark, compressed, barking.
- At `Reed` maximum, output does not clip the DAC (soft-limit at 0.90 with tanh guard).
- Bypass and hold-toggle are pop-free.
- Reference tracks: Little Walter "Juke", Sonny Boy Williamson II "Help Me".

---

## Known Limitations

- **No pitch shift.** Guitar pitch content is still guitar pitch content. Chords and
  open low strings will expose the trick; use neck pickup, tone rolled back, and
  single-note lines above the 5th fret for best results.
- **Pick transients** are faster than any real reed. `Reed` at 0.5+ smears them.
- **Stereo:** a real harp is mono. Stereo offsets are kept modest (25° tremolo
  offset, 90° chorus offset) to avoid an obvious "wide studio" feel.

---

## Future Improvements

1. Explore an envelope-follower "reed bark" mode where pick attacks briefly
   narrow Q and push the formant up the frequency sweep, mimicking the attack
   chirp of a hard-blown reed.
2. Consider a third voicing toggle that moves toward an acoustic / folk-harp
   tone (broader EQ, lighter saturation, no chorus).

---

## Related Files

- `effects/harmonica.cpp`
- `effects/wah.cpp` (Chamberlin SVF and log fc sweep reuse)
- `effects/tube_screamer.cpp` (body/edge split and soft-limit reuse)
- `effects/chorus.cpp` (fractional-delay working-buffer pattern reuse)
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
