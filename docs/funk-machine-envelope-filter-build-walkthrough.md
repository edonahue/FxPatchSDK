# Funk Machine Envelope Filter — Build Walkthrough

**File:** `effects/funk_machine_envelope_filter.cpp`  
**Date:** 2026-08-22  
**Filter/algorithm type:** linked-stereo envelope follower + Chamberlin SVF bandpass  
**Reference / inspiration:** classic Mu-Tron-style touch-sensitive funk filtering, intentionally not a component-level clone  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

## Overview

Funk Machine is a touch-sensitive resonant filter for bass, clav, clean guitar, electric piano, and synth/organ sources. Playing dynamics open the filter automatically; the Right knob / expression pedal shifts the whole envelope window so the player can move between deep bass quack and a brighter clav/lead vowel without replacing the touch response.

The first implementation deliberately favors musical behavior and low embedded cost over circuit-level Mu-Tron emulation. It reuses the repo's established Chamberlin SVF approach from `wah.cpp`, adds a local envelope detector, and keeps all state in scalar members.

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | envelope follower, Chamberlin SVF bandpass, soft limiter |
| Modeling approach | hand-tuned detector + resonant filter; no WDF circuit solve |
| Fork cross-check | no external code required; existing local wah/SVF precedent is sufficient for the first pass |
| Working buffer needed? | No |
| State variable count | linked envelope, current SVF coefficient/control countdown, four SVF states, detector coefficients, 4-state hold index (voice + direction) |
| Knob 0 (Left) | Sensitivity |
| Knob 1 (Mid) | Resonance |
| Knob 2 (Right) / Exp | Bias — shifts the envelope frequency window |
| Active LED color | Bass-Down: `kLightGreen`; GuitarKeys-Down: `kLightBlueColor`; GuitarKeys-Up: `kDarkCobalt`; Bass-Up: `kPastelGreen` |
| Bypassed LED color | Bass-Down: `kDarkLime`; GuitarKeys-Down/Up: `kDimCyan`; Bass-Up: `kDimGreen` |
| Working buffer | N/A |

## Decision 1 — Touch envelope rather than auto-LFO

The defining behavior is input dynamics, not periodic modulation. A linked stereo peak detector follows the hotter of L/R with a 4 ms attack and 95 ms release. Both channels use the same control envelope so stereo keyboards retain their image instead of having two filter cutoffs wander independently.

The detector uses a conventional asymmetric one-pole response. Its control value is then passed through a rational saturator, `x / (1 + x)`, so hot line-level keyboards do not pin the filter at the top of its range.

## Decision 2 — Chamberlin SVF bandpass (superseded 2026-08-24, see addendum)

The existing wah patch already establishes a stable, inexpensive Chamberlin state-variable filter for vocal resonant filtering. Funk Machine reuses that topology rather than introducing a new filter class.

Resonance maps to Q 1.2–7.5. This is intentionally less extreme than the wah's highest settings: an envelope filter is repeatedly hit by transients, so a slightly gentler top end is more usable and safer.

**Reusing wah's topology also meant reusing its bug.** The Chamberlin SVF's
resonant peak drifts from its target as Q drops — measured up to 165 cents
(1.65 semitones) at this effect's own Q floor of 1.2. See the 2026-08-24
addendum: this patch now shares `wah.cpp`'s replacement filter instead of its
original one.

## Decision 3 — Control layout

| Knob | Function | Mapping | Default |
|---|---|---|---|
| Left | Sensitivity | nonlinear detector gain | 0.58 |
| Mid | Resonance | Q 1.2–7.5 | 0.56 |
| Right / Exp | Bias | exponentially shifts min/max sweep frequencies | 0.40 |

Expression is intentionally **Bias**, not Mix. With expression connected, playing dynamics still animate the filter, while the player's foot moves the entire vowel range. That produces a more useful performance interaction than using the pedal only as a static wet/dry control.

## Decision 4a — Bass vs Guitar/Keys hold voice

Hold cycles through voice and direction together (see Decision 4b); the voice half of that cycle picks between two voicings:

- **Bass:** lower sweep window, 28% fixed dry foundation to retain fundamental punch.
- **Guitar/Keys:** higher sweep window, 10% dry foundation for a more obvious clav/guitar quack.

The bias control remains bounded so the maximum cutoff stays below about 3 kHz. This keeps the current Chamberlin formulation inside the range already treated as comfortable in the repo's filter guidance at high Q.

## Decision 4b — Up/Down envelope direction

Real Mu-Tron-III-family filters have a switch inverting which way the envelope drives the sweep: **Down** (this patch's original and still-default behavior) opens the filter as input gets louder; **Up** closes it instead — quiet/idle sits bright, playing harder darkens the tone. Implemented as a one-line flip of the frequency-mapping exponent (`isUp ? (1 - envNorm) : envNorm` in `processAudio`), reusing each voice's existing `fcMin`/`fcMax`/`dryFoundation` triple unchanged — direction only changes which end of the same window the envelope drives toward, not the window itself, matching how a real Up/Down switch works (it doesn't retune the filter).

**No free control-surface slot exists for a fourth action.** `endless::ActionId` only has Press and Hold; Press must stay bypass-only to match every other effect in the corpus (`patch-authoring-best-practices.md`: "Press = bypass toggle. Always."), and `handleAction` carries no timing data, so a long-hold/double-hold gesture isn't available either. Direction is folded into Hold as a 4-state Gray-code cycle instead of a plain 2-bit binary count:

```
Bass-Down (default) -> GuitarKeys-Down -> GuitarKeys-Up -> Bass-Up -> (loop)
```

Voice flips on odd-numbered holds, direction flips on even-numbered holds — every single press changes exactly one axis, never both at once. This ordering (not the more obvious `Bass-Down -> Bass-Up -> GuitarKeys-Down -> GuitarKeys-Up`) was chosen specifically because the **very first Hold press from power-on reproduces the original Bass<->GuitarKeys toggle exactly** — a player who never touches Up mode sees zero behavior change on the one gesture everyone already uses.

**Disclosed tradeoff:** rapid repeated Bass<->GuitarKeys ping-pong (a real technique — holding through a song to alternate voices) no longer round-trips in 2 presses once the cycle has passed through an Up state; the 2nd press after visiting GuitarKeys-Up lands on Bass-Up, not back on Bass-Down. This is unavoidable for any ordering of a single 4-state ring beyond the first press — no arrangement can make both axes independently round-trip in 2 steps from an arbitrary starting point. The 4 fully-distinct LED colors (see the checklist table above) are the mitigation: a player who overshoots can immediately see which of the 4 states they landed in.

**Idle-state character difference, flagged rather than pre-emptively "fixed":** in Up mode, the resting/idle cutoff (no input, `envNorm≈0`) sits near `fcMax` (~2.2–2.9 kHz depending on voice/bias) with Q as high as 7.5 — a resonant peak parked where amp hum, pickup buzz, and hiss are far more audible than Down mode's idle point near `fcMin` (70–450 Hz). This is authentic Mu-Tron Up-mode character (some players like it), not a bug, but it's genuinely untested against a noisy rig — see the hardware listening checklist below.

## Decision 5 — Control-rate filter coefficient update

An envelope filter's cutoff changes continuously, but computing `powf()` and `sinf()` on every audio sample would violate the repo's established CPU discipline. The detector still runs at audio rate; only the expensive frequency-to-SVF coefficient conversion is updated every eight samples (6 kHz control rate).

That rate is vastly faster than the 4 ms attack / 95 ms release detector can move, so the first pass should be perceptually smooth while cutting the expensive transcendental-call count by roughly 8x relative to a literal per-sample implementation.

Hardware listening must still check for zippering on sharp clav/guitar transients. If audible, the next experiment should interpolate `f1` between control points rather than simply increasing transcendental-call frequency.

**Why this deviates from every other effect's "once per `processAudio` call" pattern (kept deliberately, not an oversight).** Every other coefficient-recompute site in this corpus — including `wah.cpp`, the file this patch's SVF is built on — computes its expensive coefficients once per `processAudio` call, not at a sub-block interval (`patch-authoring-best-practices.md` §6's `dimension_chorus.cpp` example is the corpus's canonical "don't call `powf` every sample" fix, and it resolves to the same once-per-block pattern). This patch is the one deliberate exception: the SDK does not document or guarantee a block size to patches, so relying on "once per call" gives an *undefined* cutoff-tracking rate — fine for a slowly-moving, user-driven parameter like wah's expression sweep, but not obviously fine for a touch envelope that needs to track playing dynamics faster than an unknown host block size might otherwise allow. A self-controlled fixed interval (every 8 samples, ~167 µs) gives a deterministic tracking rate regardless of host block size. The added cost is small in absolute terms — roughly 16-31 cycles/sample amortized, ~0.1-0.2% of the working 15k-cycle/sample ceiling (see `docs/cycle-budget.md`) — so this is a pattern-consistency question the corpus hadn't needed to answer before, not a CPU-budget one. See `patch-authoring-best-practices.md` §6 for a pointer back to this reasoning.

## Decision 6 — Gain staging (superseded 2026-08-24, see addendum)

Raw Chamberlin bandpass level varies with Q, so the output is first normalized by approximately `2 / Q`, then given a moderate 1.75x vocal boost. Bass mode mixes in a fixed dry foundation before the final `dsp::softLimit` safety stage.

The limiter is an airbag, not the effect's creative nonlinearity.

**The `2/Q` premise was wrong**, the same bug found and fixed first in
`wah.cpp`: the raw Chamberlin peak is actually `Q`, not `Q/2`, so `2/Q`
happened to cancel it by coincidence rather than by the design this section
describes — giving an actual old peak of `2 × 1.75 = 3.5`, constant across
Q, not the Q-dependent value the formula's shape suggests. See the
addendum for the fix and the recalibration.

## Primitive reuse policy

The envelope follower remains local in this patch for now. The backlog anticipates that Bass Glue and Attack/Decay may need a closely related detector. If a second implementation converges on the same attack/release detector behavior, extract it into `source/dsp/` then, with unit tests. Do not create a general detector library in advance.

## State clearing

| Event | Clear? | Reason |
|---|---|---|
| Bypass -> active | Yes, detector + filters | avoids stale envelope/filter state |
| Active -> bypass | No | pass-through requires no processing |
| Hold press (voice or direction step) | Filter state only | mapping window or exponent changes, not the SVF topology; retaining detector energy keeps the gesture natural. Direction changes are, if anything, a stronger case for this than voice changes -- inverting the exponent at extreme envNorm produces the largest single fc jump this design can produce, the same class of transient `wah.cpp`'s own mode-toggle clearing exists to prevent |
| Knob/expression movement | No | continuity is part of the effect |

## Initial hardware listening checklist

- [ ] Bass mode preserves low-E fundamentals and does not become thin at high resonance.
- [ ] Sensitivity has useful behavior across passive bass/guitar and line-level keyboards.
- [ ] Clav-style playing produces a fast, vocal `quack` rather than a slow wah swell.
- [ ] Sustained electric-piano chords settle naturally as the release closes the filter.
- [ ] Expression Bias feels complementary to touch dynamics rather than fighting them.
- [ ] 6 kHz coefficient control rate produces no audible stepping.
- [ ] Guitar/Keys mode is clearly distinct from Bass mode.
- [ ] High resonance remains stable at every Bias setting.
- [ ] Stereo keyboard input does not pull the image left/right as the envelope changes.
- [ ] Bypass and hold changes are click-free.
- [ ] Up mode's inverted sweep feels musically distinct from Down, not just measurably different.
- [ ] Up mode's idle resonant peak (parked near `fcMax`) doesn't ring audibly on a noisy bass/guitar rig -- if it does, the fix is a small idle-Q or idle-gain trim, not a change to `fcMin`/`fcMax` (see Decision 4b).
- [ ] The 4-state Hold cycle is discoverable without a manual -- confirm the 4 LED colors read as clearly distinct at a glance, including under stage lighting.

## Next tuning questions

1. Is 4 ms attack fast enough for clav/slap, or should the detector move toward 2–3 ms?
2. Is 95 ms release funky and articulate, or too short for electric piano?
3. Should Bass mode use a lowpass/bandpass blend instead of a fixed dry foundation?
4. Does Sensitivity need source-specific gain compensation between Bass and Guitar/Keys modes?
5. Does the coefficient update need interpolation on hardware?
6. **Wet/dry mix control was considered and explicitly deferred, not overlooked.** The original patch backlog offered knob 2 as either "Filter bias or dry/wet blend"; this pass keeps Bias only, so the dry/wet balance stays a fixed per-voice constant (28% dry in Bass, 10% in Guitar/Keys) with no player-facing mix control at all. If hardware listening finds the fixed blend too limiting, the natural next step is deciding what to trade for it -- Bias moving to expression-only, or mix becoming a Hold-modified secondary behavior -- not a fast unilateral addition, since the control surface is already at capacity (3 knobs, and Hold now carries 4 states).
7. A third "Clav" voice was also considered and explicitly deferred. `docs/patch-backlog.md`'s own "Clav Wah" entry (#10) already suggests folding it in here ("likely better as a Funk Machine mode") rather than shipping as a separate patch -- worth revisiting once Bass/Guitar+Keys/Up/Down has real hardware feedback, not before.

## Reproducing the acceptance test

```bash
g++ -std=c++20 -O2 -fsingle-precision-constant -Wall -Wextra -I source \
    tests/funk_machine_acceptance_test.cpp -o /tmp/fm_accept && /tmp/fm_accept
```

## Related files

- `effects/funk_machine_envelope_filter.cpp`
- `effects/wah.cpp`
- `effects/harmonica.cpp` — the third effect sharing this same detuning bug,
  fixed the same day with the same primitive
- `source/dsp/biquad.h` / `source/dsp/filter_coeff.h` — the RBJ bandpass
  biquad primitive this patch now shares with `wah.cpp` and `harmonica.cpp`;
  `filter_coeff.h`'s `svfF1` is the Chamberlin coefficient this patch used to
  inline and no longer does
- `tests/funk_machine_acceptance_test.cpp`
- `tests/funk_machine_biquad_accuracy_probe.cpp` — the before/after
  accuracy measurement for the 2026-08-24 filter fix
- `docs/patch-backlog.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/patch-authoring-best-practices.md`
- `docs/wah-build-walkthrough.md` — the sibling effect where this same fix
  was designed first, with the fuller derivation

---

## 2026-08-24 — applying the reverse-engineering findings

[`docs/endl-corpus-study.md`](endl-corpus-study.md) established that no Polyend
factory plate and no Playground patch links a newlib transcendental, while all
fourteen of ours did. That was left as a documented prior. This patch is where
it was acted on, as a worked example.

### What the control chain cost

The frequency map ran `powf()` then `sinf()` every eight samples. The 8-sample
interval was chosen deliberately (Decision 5) to keep transcendentals out of the
per-sample path — and it does — but 8 samples at 48 kHz is still a 6 kHz control
rate, so **12,000 newlib calls per second**. The comment above
`kControlInterval` claimed to be "avoiding powf/sinf in the inner loop", which
was true and beside the point.

### The rewrite

Two observations collapse the chain to arithmetic:

1. The windows are built as `70 * powf(4, bias)` and friends. `4`, `2.4`, `3`
   and `1.8` are literals, so `powf(base, bias)` is `exp2(bias * log2(base))`
   with a **constant** multiplier. Carrying the window as `log2(Hz)` makes the
   whole map affine in `bias`, so no runtime logarithm is needed either — the
   pow disappears without a log taking its place.
2. `fc` never exceeds ~2.9 kHz, so `pi*fc/fs` stays below 0.19 rad, where
   `sin(x) = x - x³/6` has its first omitted term below `2.1e-6`.

Per control tick that leaves one `exp2` — a degree-5 polynomial plus an
IEEE-754 exponent-field write — and three multiplies.

### Accuracy, measured before adopting

[`tests/funk_machine_approx_probe.cpp`](../tests/funk_machine_approx_probe.cpp)
sweeps both voices across the full bias × envelope space, 80,802 points:

| | |
|---|---|
| worst relative error in `f1` | `8.8e-05` |
| worst cutoff error | 0.18 Hz |
| **worst cutoff error** | **0.15 cents** |

Pitch discrimination tops out near 1 cent and filter cutoff is far less
sensitive than pitch, so this is comfortably inaudible. End-to-end, the probe
sweeps move by at most **0.024 dB** with no flag changes.

### Result

| | before | after |
|---|---|---|
| image size | 10,244 B | **4,508 B** |
| libm routines linked | 13 | 4 |
| transcendental calls/sec | ~12,000 | 0 |
| `bl` into libm from `processAudio` | `powf`, `sinf`, `tanhf`, `fabsf`, `fmaxf` | `tanhf` only |

It is now the smallest effect in the repo — the next smallest is 5,588 bytes —
and sits inside Polyend's own factory range of 3,012–12,984.

### The tanhf that was left alone

The only newlib call remaining is the `tanhf` inside `dsp::softLimit`.
`Malleus_Fuzz` uses a cascaded `x/(1+|x|)` instead, so the swap was the obvious
next move. It was measured and **declined**.

[`tests/funk_machine_limiter_probe.cpp`](../tests/funk_machine_limiter_probe.cpp)
drives the effect at maximum sensitivity and resonance and counts how often the
limiter reaches its nonlinear region:

| input level | output peak | samples over threshold |
|---|---|---|
| 0.25x | 0.196 | 0 / 51,200 |
| 1.0x nominal | 0.554 | 0 / 51,200 |
| 4.0x (deliberate overload) | 1.000 | 21,730 / 51,200 (42%) |

`dsp::softLimit` passes `|x| <= 0.90` through untouched, so at real operating
levels **the tanhf is never called** — it is linked but cold, and the
per-sample cost is a single compare. Swapping it would buy ~2,900 bytes of
image in a patch already at 0.86% of the 512 KB region, in exchange for
changing the safety limiter's character. Not worth it.

The honest conclusion is that "zero libm" is not the goal; *not calling libm on
the audio path* is. This patch reaches that, and the linked-but-cold remainder
is left where it is.

### The Chamberlin SVF detuning, fixed (later the same day)

The work above removed `powf`/`sinf` from the control chain but left the
*filter itself* unchanged — and it turned out to have a real accuracy bug,
found while applying this same methodology to `wah.cpp` next. Driving the
actual per-sample recursion with swept sine tones and measuring where the
output truly peaks (not the pole angle, which diverges from the true peak for
this filter's zero structure at low Q) shows the resonant frequency drifts
from its target as Q drops — **165 cents (1.65 semitones) sharp** at this
effect's own Q floor of 1.2, `fc≈2900 Hz`. Documented DSP-literature
limitation of the Chamberlin SVF (Lazzarini & Timoney, arXiv:2111.05592), not
specific to this codebase — see `wah-build-walkthrough.md`'s 2026-08-24
addendum, where the fix was designed first.

**Fix:** replaced the Chamberlin SVF with the same RBJ constant-peak-gain
bandpass biquad `wah.cpp` now uses
(`dsp::rbjBandpassCoeffs`/`dsp::BandpassBiquad`,
[`source/dsp/biquad.h`](../source/dsp/biquad.h)) — with one difference. This
patch's control chain is libm-free (the work above), and RBJ's coefficient
formula needs both `sin(w0)` and `cos(w0)` at `w0 = 2π·fc/fs` — double the
angle the existing `sinSmall` was validated for. Rather than fit a new series
over the wider range, `sin(w0)`/`cos(w0)` are derived via double-angle
identities from `sinSmall(w0/2)`/a new `cosSmall(w0/2)` — `w0/2` is exactly
this patch's own existing half-angle, so the already-validated `<0.19` rad
range still applies.

Verified end-to-end with the exact z-transform magnitude response `|H(f)|`,
not a time-domain peak search — for a biquad, `|H(f)|` computed from the
coefficients *is* the frequency response, not a proxy for it, so this is more
exact and sidesteps settling/grid artifacts entirely.
[`tests/funk_machine_biquad_accuracy_probe.cpp`](../tests/funk_machine_biquad_accuracy_probe.cpp)
measures **1.387 cents** worst-case error across this effect's full `fc`
(70–2900 Hz) and `Q` (1.2–7.5) range — consistent with float32 rounding in
the coefficients themselves, not the small-angle approximation.

**Gain staging recalibrated for the same reason found in `wah.cpp`.**
Decision 6's `2/Q` normalization assumed the raw Chamberlin peak was `Q/2`;
direct measurement shows it is `Q`. The old formula's `Q`-dependence
happened to cancel the `2/Q` term anyway, so the *actual* old peak was
already `Q`-independent — `2 × 1.75 = 3.5`, not the `Q`-dependent value the
formula's shape suggested. `kResonanceGain` is now `3.5` directly, matching
real old loudness rather than the formula's never-quite-true intent.

Measured, not assumed: `scripts/analyze_effects.py` field-by-field against
the pre-fix build shows default-settings loudness essentially unchanged
(`fundamental_gain` 2.767931 → 2.794333, +0.95%) — and, as a side effect of
the more accurate filter, **lower** distortion at default settings
(`spectral.thd_percent` 0.0955% → 0.0394%, THD roughly halved). No new or
lost qualitative flags. A dedicated stability sweep (extreme sensitivity/
resonance/bias corners, hold-cycling through all four voice/direction states
every 8 blocks, 20× input overload) stayed bounded and finite throughout.

Image size: 4,508 B → 4,852 B (+344 B, from the added `cosSmall` and the
biquad's extra state-update multiply) — still the smallest or
near-smallest effect in the repo, comfortably inside Polyend's factory
range.

Not revisited in this pass: the `tanhf` conclusion above (declined, cold
limiter) still holds — the biquad swap didn't change how hard the limiter
gets driven at realistic levels, only how accurately the filter is tuned.
