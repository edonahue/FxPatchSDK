# Curated Patch Backlog

**Status:** idea backlog, not an instruction to expand the catalog indiscriminately.

This document captures candidate Polyend Endless patches that would add genuinely new musical territory to the current effect library. It complements the existing catalog-freeze guidance: an entry here is a design candidate, not approval to implement it.

The emphasis is on patches that are useful across guitar, bass, and stereo keyboards, fit the Endless control surface well, and either reuse the existing DSP vocabulary or justify a small new primitive through repeated use.

## Scoring

Scores are 1–5, where 5 is strongest/best except DSP risk, where 5 means highest implementation risk.

- **Musical fit** — usefulness and distinctiveness across guitar, bass, and keyboards.
- **Catalog novelty** — how much new territory it adds versus the existing effects.
- **Reuse value** — value of its DSP/control work to later patches.
- **DSP risk** — implementation and tuning difficulty on Endless hardware; lower is safer.
- **Hardware-listening priority** — how important real-pedal auditioning is before considering the patch successful.

| Priority | Candidate | Primary territory | Musical fit | Catalog novelty | Reuse value | DSP risk | HW listening | Likely new primitive |
|---|---|---|---:|---:|---:|---:|---:|---|
| 1 | Funk Machine Envelope Filter | funk bass, clav, clean guitar | 5 | 5 | 5 | 3 | 5 | envelope follower / detector smoothing |
| 2 | Suitcase CP Preamp | Rhodes/Wurli, keys, clean guitar | 5 | 5 | 4 | 2 | 5 | none initially; reuse saturation/LFO/filter pieces |
| 3 | Tiki Spring Tank | spring ambience, surf/lounge, keys | 5 | 5 | 5 | 4 | 5 | reverb network only if later reused |
| 4 | Pirate Radio Echo | tape/BBD/dub delay | 5 | 4 | 5 | 3 | 5 | feedback EQ / wow-flutter only if duplicated later |
| 5 | Bass Glue Preamp | bass dynamics and tone | 5 | 5 | 5 | 3 | 5 | envelope follower may share work with Funk Machine |
| 6 | Tape Pre / Warped Deck | degraded preamp and movement | 4 | 4 | 4 | 3 | 5 | wow/flutter modulation if later reused |
| 7 | Juno Ensemble | synth/string-machine ensemble | 4 | 3 | 3 | 3 | 5 | none unless it exposes a reusable ensemble topology |
| 8 | Rotary Lite | organ/guitar rotary movement | 4 | 5 | 4 | 4 | 5 | rotor modulation components only after duplication |
| 9 | Attack/Decay Swell | auto-volume, pads, ambient guitar | 4 | 5 | 5 | 4 | 5 | envelope follower can reuse Funk Machine work |
| 10 | Clav Wah | clav/bass percussive filter | 4 | 3 | 3 | 2 | 4 | none; likely better as a Funk Machine mode |

## 1. Funk Machine Envelope Filter

**Why it belongs:** The catalog has a manually swept wah but no touch-sensitive funk filter. This adds a distinctly different performance behavior and works naturally with bass, clav, electric piano, and clean guitar.

**Target character:** Mu-Tron-family musicality rather than an exact circuit clone unless later research identifies a compelling circuit-modeling path. The important behavior is a responsive envelope, controllable resonance, and a useful low-frequency voicing for bass.

**Candidate controls**

- Left — Sensitivity / envelope depth
- Mid — Resonance / vowel color
- Right / Expression — Filter bias or dry/wet blend
- Hold — Bass / Guitar+Keys voicing

**Engineering value:** Strong candidate to introduce an envelope follower/detector primitive. Do not extract it to `source/dsp/` merely because this patch needs one; keep it local until a second patch (likely Bass Glue or Attack/Decay) proves the duplication threshold.

**Key validation:** attack response at different input levels; no zippering; bass fundamentals remain convincing; expression behavior remains useful rather than fighting the envelope.

## 2. Suitcase CP Preamp

**Why it belongs:** A keyboard-first processor is a major gap in the current catalog. This should make electric-piano sources feel more physical without becoming another generic overdrive.

**Target character:** suitcase-electric-piano preamp coloration: gentle bark, broad tone shaping, and stereo tremolo/autopan. A hold voice can move between a round Rhodes-like treatment and a more aggressive Wurli-like treatment without claiming component-level emulation unless supported by research.

**Candidate controls**

- Left — Bark / drive
- Mid — Tremolo rate or tone
- Right / Expression — Tremolo depth / stereo width
- Hold — Rhodes / Wurli voicing

**Engineering value:** Mostly composed from already-understood nonlinear, filter, LFO, smoother, and stereo-control idioms. That makes it a good low-risk musical patch after the envelope-filter work.

**Key validation:** preserves keyboard transient dynamics; stereo motion does not collapse unpleasantly in mono; drive range stays useful for guitar as well as line-level keys.

## 3. Tiki Spring Tank

**Why it belongs:** The catalog has no reverb. A deliberately characterful spring effect adds more value than a generic hall and suits guitar, organ, electric piano, and sparse bass accents.

**Target character:** drippy spring/tank response with a dark vintage preamp path. The goal is musical splash and decay character rather than maximum realism or longest possible tail.

**Candidate controls**

- Left — Dwell
- Mid — Tank tone / darkness
- Right / Expression — Mix
- Hold — Surf / Haunted Lounge voicing

**Engineering value:** Exercises the large working buffer in a new way and establishes a time-domain reverb pattern. Avoid prematurely building a generalized reverb library.

**Key validation:** stable feedback at every setting; no runaway energy; convincing transient splash; useful subtle settings as well as exaggerated ones; CPU and buffer use documented.

## 4. Pirate Radio Echo

**Why it belongs:** `back_talk_reverse_delay.cpp` covers reverse delay, not a conventional playable echo. A dark tape/BBD/dub processor would be useful on nearly every instrument class.

**Target character:** aged, bandwidth-limited repeats with mild saturation and restrained wow/flutter. Hold should create a genuinely different performance state rather than a cosmetic voice.

**Candidate controls**

- Left — Time
- Mid — Feedback
- Right / Expression — Mix
- Hold — Tape / Dub-Worn voice, or a safely bounded feedback-push behavior if hardware interaction proves good

**Engineering value:** Reuses delay/ring-buffer infrastructure while adding feedback-path voicing. Any self-oscillation behavior must remain bounded and safe.

**Key validation:** no unstable feedback corner; smooth delay-time changes or intentionally musical pitch movement; useful expression sweep; repeat degradation accumulates naturally.

## 5. Bass Glue Preamp

**Why it belongs:** Most existing nonlinear patches are guitar-pedal emulations. Bass deserves a utility patch that protects fundamentals and adds dynamics control rather than simply clipping harder.

**Target character:** compressor, broad tilt/contour EQ, and mild preamp saturation with enough clean path to preserve articulation.

**Candidate controls**

- Left — Compression
- Mid — Tone / tilt
- Right / Expression — Blend or output
- Hold — Studio / Amp voice

**Engineering value:** Likely second consumer of envelope detection after Funk Machine. If both implementations converge on the same detector/smoother shape, that is the point to consider extraction into `source/dsp/` with unit tests.

**Key validation:** low-E fundamentals remain intact; gain compensation is sensible; attack/release behavior does not pump on ordinary bass lines; line-level keyboards do not overload the detector path.

## 6. Tape Pre / Warped Deck

**Why it belongs:** Adds a broad character processor rather than another named overdrive circuit.

**Target character:** compressed preamp saturation, softened high end, subtle wow/flutter, and an optional deliberately worn voice.

**Candidate controls**

- Left — Drive
- Mid — Age / wobble
- Right / Expression — Blend
- Hold — Clean Tape / Warped Deck

**Engineering value:** Useful sandbox for combining modulation and nonlinearity without a long feedback network.

**Key validation:** modulation remains subtle through most of the range; saturation does not become a generic fuzz; stereo input remains coherent.

## 7. Juno Ensemble

**Why it belongs:** Potentially useful for pads, strings, organ, and clean guitar, but it must be demonstrably different from both `chorus.cpp` and `dimension_chorus.cpp` before implementation.

**Target character:** synth/string-machine ensemble rather than stompbox chorus: broad stereo animation with little obvious pitch wobble.

**Candidate controls**

- Left — Rate
- Mid — Ensemble depth
- Right / Expression — Width
- Hold — Ensemble I / II

**Gate before implementation:** Write a short architecture comparison against the two existing chorus patches. If the proposed signal topology is mostly a retune, reject this candidate.

## 8. Rotary Lite

**Why it belongs:** Organ and keyboard sources are underserved, and a playable rotary treatment would also work for guitar.

**Target character:** convincing musical impression of a rotary speaker through coordinated amplitude modulation, filtering, stereo motion, and restrained Doppler/pitch movement rather than an expensive physical model.

**Candidate controls**

- Left — Drive
- Mid — Speed / acceleration behavior
- Right / Expression — Spread / rotor intensity
- Hold — Slow / Fast

**Engineering value:** Good integration challenge for existing LFO, filter, delay, and smoothing tools.

**Key validation:** slow/fast transition is musical; no seasick pitch modulation; mono compatibility; CPU measured before adding sophistication.

## 9. Attack/Decay Swell

**Why it belongs:** Adds a performance transformation unavailable elsewhere in the catalog and can turn guitar into pad-like material without requiring pitch detection.

**Target character:** automatic attack removal and controllable bloom, optionally with a more exaggerated reverse-like voice.

**Candidate controls**

- Left — Attack
- Mid — Decay / sustain behavior
- Right / Expression — Mix
- Hold — Swell / Reverse-ish

**Engineering value:** Another likely consumer of the envelope follower developed for Funk Machine, strengthening the case for eventual shared DSP extraction.

**Key validation:** reliable retriggering across guitar, bass, and keyboard transients; no clicks; legato behavior is intentional; detector does not chatter around threshold.

## 10. Clav Wah

**Why it belongs:** Percussive clav/filter treatment is musically attractive, but this probably should not become an independent patch unless it proves substantially different from Funk Machine.

**Preferred direction:** prototype it as the alternate voice or a tuning experiment of Funk Machine first.

**Candidate controls**

- Left — Sensitivity
- Mid — Bite / resonance
- Right / Expression — Manual filter position
- Hold — Auto / Manual

## Recommended sequence

The backlog should be explored in this order unless new hardware evidence changes the priorities:

1. **Funk Machine Envelope Filter** — broadest musical payoff and establishes envelope-detection groundwork.
2. **Suitcase CP Preamp** — keyboard-first, comparatively low implementation risk, immediately distinct from the catalog.
3. **Tiki Spring Tank** — establishes the missing reverb/time-domain family.
4. **Pirate Radio Echo** — builds on existing delay experience with a conventional performance-oriented echo.
5. **Bass Glue Preamp** — converts the envelope work into a practical dynamics patch and may justify a shared detector primitive.

After those, reassess the catalog before implementing more. The purpose of this backlog is to preserve good ideas while keeping the shipped patch set intentional.

## Before promoting any candidate to implementation

For each candidate:

1. Confirm it adds a materially new musical behavior to `effects/README.md`.
2. Complete the pre-flight table from `docs/templates/patch-build-walkthrough.md`.
3. Identify the closest existing patches and explicitly state why this is not merely a retune.
4. Keep new DSP local unless at least two effects prove the same primitive is needed.
5. Define a host-side behavioral test before implementation where practical.
6. Run syntax/lint, DSP tests, real ARM builds, and effect analysis in the repository's documented order.
7. Treat real Endless hardware listening as part of acceptance, especially for modulation, dynamics, and time-domain patches.
8. Record CPU measurements before adopting expensive nonlinear solving, oversampling, or unusually dense reverb/modulation structures.
