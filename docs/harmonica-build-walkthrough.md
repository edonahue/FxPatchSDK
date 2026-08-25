# Harmonica (Blues Bullet-Mic) — Build Walkthrough

**File:** `effects/harmonica.cpp`
**Date:** 2026-04-16
**Filter/algorithm type:** dual RBJ bandpass biquad formants (Chamberlin SVF until 2026-08-24, see dated addendum) + asymmetric tanh reed saturation with body/edge split + post-LPF + AM tremolo + fractional-delay micro-chorus
**Reference voice / inspiration:** Shure Green Bullet cupped into a cranked tube amp — Chicago blues harmonica (Little Walter, Sonny Boy Williamson II)
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

> **2026-06-13 update:** the 1-pole filter helpers, the safety soft-limit,
> and the DC blocker now come from [`source/dsp/`](../source/dsp). The DC
> blocker switched from inline state arrays to two `dsp::DcBlocker` instances
> with alpha set per block. Behavior is unchanged.

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
resonant bandpass (cup) with a fixed one at ~1.7 kHz (nasal) produces
the two-peak vowel structure that reads as "harp" rather than "filtered guitar."

**2026-08-24:** both formants were originally Chamberlin SVFs and are now RBJ
bandpass biquads — see the dated addendum near the end of this document. The
two-formant *idea* above is unaffected; what changed is how accurately each
formant's peak lands where the Q/fc knobs say it should.

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
| Formant-1 peak gain | ×6.0 (+15.6 dB) | ×8.4 (+18.5 dB) |
| Formant-2 mix | 0.35 | 0.50 |
| Pre-HP fc | 120 Hz | 160 Hz |
| Low-shelf cut | −2 dB | −5 dB |
| Saturation clipGain | 2.2 | 3.4 |
| Asymmetry | 1.10 | 1.30 |
| Post-LP fc | 2.8 kHz | 1.9 kHz |
| Tremolo depth | ±0.8 dB | ±1.4 dB |
| µ-chorus wet | 0.08 | 0.16 |
| WAA fc_max | 2500 Hz | 2000 Hz |

*Formant-1 peak gain corrected 2026-08-24.* The `Voicing` struct's `form1Gain`
field is still ×3.0/×4.2 — its own comment ("peak amplitude after unity-peak
normalization") assumed the Chamberlin bandpass's raw peak was `Q/2`; direct
measurement showed it is `Q`, so the actual peak was always `2×form1Gain`
(×6.0/×8.4), not the ×3.0/×4.2 this table originally stated. See the dated
addendum — this is the same gain-staging bug found first in `wah.cpp`, and
the *sound* is unchanged (the code always produced ×6.0/×8.4; only this
table's arithmetic was wrong).

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
bash tests/check_arm_build.sh
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

## 2026-08-24 — fixing the formant filters' Q-dependent detuning

Third and last effect fixed for the Chamberlin SVF's resonant-peak accuracy
problem, found first in `wah.cpp` and fixed next in
`funk_machine_envelope_filter.cpp`. This effect's exposure was the mildest of
the three — its Q floor (2.0 for formant-2, 3.0 for formant-1 Open) is well
above wah's 1.0 and funk_machine's 1.2 — but still real and still measured
rather than assumed.

**Detuning.** Driving both formants' actual per-sample recursions with swept
sine tones and measuring where the output truly peaks: **51 cents** worst
case (formant-1 at Open's toe, `fc=2500 Hz, Q=3.0`; formant-2 at its fixed
`fc=1700 Hz, Q=2.0`). Documented DSP-literature limitation of the Chamberlin
SVF (Lazzarini & Timoney, arXiv:2111.05592) — see `wah-build-walkthrough.md`'s
2026-08-24 addendum for the full derivation.

**Fix:** both formants now use the same `dsp::rbjBandpassCoeffs`/
`dsp::BandpassBiquad` primitive as `wah.cpp` and
`funk_machine_envelope_filter.cpp`
([`source/dsp/biquad.h`](../source/dsp/biquad.h)). Both formants are computed
at block rate already (once per `processAudio` call, same as `wah.cpp`), so
no libm-free treatment was needed here the way it was for
`funk_machine_envelope_filter.cpp`'s faster control rate.

Verified with the exact z-transform magnitude response `|H(f)|`, the same
method used for the other two fixes and for the same reason: it computes the
actual frequency response, not a proxy for it.
[`tests/harmonica_biquad_accuracy_probe.cpp`](../tests/harmonica_biquad_accuracy_probe.cpp)
measures **0.060 cents** worst-case error across both formants' full reachable
range.

**Gain staging.** Same root cause as the other two effects: `bp1Gain`'s
`(2/Q)` correction assumed a raw Chamberlin peak of `Q/2`; direct measurement
shows it is `Q`, so the correction always cancelled `Q`-dependence anyway.
Unlike `wah.cpp`/`funk_machine_envelope_filter.cpp`, this meant **no actual
gain change** here — `2 × q1 × form1Gain` and the new `2 × form1Gain` are
algebraically identical once `q1 = 1/Q` is substituted, for any `Q`, so the
simplification just removes a dead multiply rather than recalibrating a
constant. Decision 5's peak-gain table above was corrected to state the real
(and always-true) ×6.0/×8.4 figures, since it had been computed from the
`Voicing` struct's `form1Gain` field alone.

Measured, not assumed: `scripts/analyze_effects.py` field-by-field against
the pre-fix build shows the smallest delta of the three effects fixed this
session — the largest field movement is 10% on a minor `hot_sample_ratio`
metric; default-settings loudness and THD are both within 0.2% of the
pre-fix values (`spectral.thd_percent` 7.816% → 7.807%). **All three
pre-existing qualitative flags are unchanged**, including "THD 7.8% is high
for a filter effect" and "spurious spectral energy −18.2 dB is high" — both
already documented as this patch's intended reed-saturation character, not a
defect, and this fix doesn't touch that. A dedicated stability sweep (extreme
tone/reed/waa corners, voicing-toggle cycling every 8 blocks, 20× input
overload) stayed bounded and finite throughout.

## Related Files

- `effects/harmonica.cpp`
- `effects/wah.cpp` (log fc sweep reuse; also the sibling effect where the
  2026-08-24 filter fix was designed first, with the fuller derivation)
- `effects/funk_machine_envelope_filter.cpp` (the other sibling fix,
  including the libm-free coefficient variant this effect didn't need)
- `effects/tube_screamer.cpp` (body/edge split and soft-limit reuse)
- `effects/chorus.cpp` (fractional-delay working-buffer pattern reuse)
- `source/dsp/biquad.h` / `source/dsp/filter_coeff.h` — the RBJ bandpass
  biquad primitive both formants now use
- `tests/harmonica_biquad_accuracy_probe.cpp` — the accuracy measurement for
  the 2026-08-24 fix
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
