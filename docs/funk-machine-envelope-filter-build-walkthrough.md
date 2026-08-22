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
| State variable count | linked envelope, current SVF coefficient/control countdown, four SVF states, detector coefficients |
| Knob 0 (Left) | Sensitivity |
| Knob 1 (Mid) | Resonance |
| Knob 2 (Right) / Exp | Bias — shifts the envelope frequency window |
| Active LED color | Bass: `Color::kLightGreen`; Guitar/Keys: `Color::kLightBlueColor` |
| Bypassed LED color | Bass: `Color::kDimGreen`; Guitar/Keys: `Color::kDimCyan` |
| Working buffer | N/A |

## Decision 1 — Touch envelope rather than auto-LFO

The defining behavior is input dynamics, not periodic modulation. A linked stereo peak detector follows the hotter of L/R with a 4 ms attack and 95 ms release. Both channels use the same control envelope so stereo keyboards retain their image instead of having two filter cutoffs wander independently.

The detector uses a conventional asymmetric one-pole response. Its control value is then passed through a rational saturator, `x / (1 + x)`, so hot line-level keyboards do not pin the filter at the top of its range.

## Decision 2 — Chamberlin SVF bandpass

The existing wah patch already establishes a stable, inexpensive Chamberlin state-variable filter for vocal resonant filtering. Funk Machine reuses that topology rather than introducing a new filter class.

Resonance maps to Q 1.2–7.5. This is intentionally less extreme than the wah's highest settings: an envelope filter is repeatedly hit by transients, so a slightly gentler top end is more usable and safer.

## Decision 3 — Control layout

| Knob | Function | Mapping | Default |
|---|---|---|---|
| Left | Sensitivity | nonlinear detector gain | 0.58 |
| Mid | Resonance | Q 1.2–7.5 | 0.56 |
| Right / Exp | Bias | exponentially shifts min/max sweep frequencies | 0.40 |

Expression is intentionally **Bias**, not Mix. With expression connected, playing dynamics still animate the filter, while the player's foot moves the entire vowel range. That produces a more useful performance interaction than using the pedal only as a static wet/dry control.

## Decision 4 — Bass vs Guitar/Keys hold voice

Hold toggles two voicings:

- **Bass:** lower sweep window, 28% fixed dry foundation to retain fundamental punch.
- **Guitar/Keys:** higher sweep window, 10% dry foundation for a more obvious clav/guitar quack.

The bias control remains bounded so the maximum cutoff stays below about 3 kHz. This keeps the current Chamberlin formulation inside the range already treated as comfortable in the repo's filter guidance at high Q.

## Decision 5 — Control-rate filter coefficient update

An envelope filter's cutoff changes continuously, but computing `powf()` and `sinf()` on every audio sample would violate the repo's established CPU discipline. The detector still runs at audio rate; only the expensive frequency-to-SVF coefficient conversion is updated every eight samples (6 kHz control rate).

That rate is vastly faster than the 4 ms attack / 95 ms release detector can move, so the first pass should be perceptually smooth while cutting the expensive transcendental-call count by roughly 8x relative to a literal per-sample implementation.

Hardware listening must still check for zippering on sharp clav/guitar transients. If audible, the next experiment should interpolate `f1` between control points rather than simply increasing transcendental-call frequency.

## Decision 6 — Gain staging

Raw Chamberlin bandpass level varies with Q, so the output is first normalized by approximately `2 / Q`, then given a moderate 1.75x vocal boost. Bass mode mixes in a fixed dry foundation before the final `dsp::softLimit` safety stage.

The limiter is an airbag, not the effect's creative nonlinearity.

## Primitive reuse policy

The envelope follower remains local in this patch for now. The backlog anticipates that Bass Glue and Attack/Decay may need a closely related detector. If a second implementation converges on the same attack/release detector behavior, extract it into `source/dsp/` then, with unit tests. Do not create a general detector library in advance.

## State clearing

| Event | Clear? | Reason |
|---|---|---|
| Bypass -> active | Yes, detector + filters | avoids stale envelope/filter state |
| Active -> bypass | No | pass-through requires no processing |
| Bass/Guitar+Keys hold toggle | Filter state only | frequency window changes; retaining detector energy keeps the gesture natural |
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

## Next tuning questions

1. Is 4 ms attack fast enough for clav/slap, or should the detector move toward 2–3 ms?
2. Is 95 ms release funky and articulate, or too short for electric piano?
3. Should Bass mode use a lowpass/bandpass blend instead of a fixed dry foundation?
4. Does Sensitivity need source-specific gain compensation between Bass and Guitar/Keys modes?
5. Does the coefficient update need interpolation on hardware?

## Related files

- `effects/funk_machine_envelope_filter.cpp`
- `effects/wah.cpp`
- `docs/patch-backlog.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/patch-authoring-best-practices.md`
