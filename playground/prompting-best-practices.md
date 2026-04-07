# Playground Prompting Best Practices

Notes collected from SpiralCaster's 23-effect session and general Playground experimentation.
The Playground pipeline parses your text, selects DSP algorithms, generates C++ code, and
runs automated tests — understanding how it reads prompts helps you get better results faster.

---

## What the pipeline needs to know

To build a working patch, Playground needs to resolve:

1. **Signal flow** — what processing stages happen in what order
2. **Parameter assignment** — what each of the three knobs controls, with ranges
3. **Footswitch behavior** — what short press and long press do
4. **Expression pedal** — what parameter it controls and over what range
5. **LED state** — what colors reflect what states

If your prompt leaves any of these ambiguous, the pipeline will make a default choice. That
default may not be what you wanted. The revision loop exists to correct these — plan on 1–3
rounds for anything complex.

---

## Structure prompts around the three knobs

The Endless has exactly three knobs. Name them explicitly:

```
Knob 1 controls X from Y to Z.
Knob 2 controls A from B to C.
Knob 3 controls D from E to F.
```

Or by label:
```
Param 1 = delay time (20–800 ms)
Param 2 = distortion gain (0–30 dB)
Param 3 = HPF cutoff (20–1200 Hz)
```

If you don't name knobs, the pipeline guesses. You may get sensible defaults (BELLOWS got
SizeDec / Thresh / Tone from "Reverb size and decay, Gating and ducking threshold, tone")
but explicit names always produce more predictable results.

**Provide units and ranges.** "Rate 0.05–8.0 Hz" is unambiguous. "Rate control" is not.

---

## Be explicit about footswitch behavior

The Endless has one footswitch with short press and long press.
State what each does — don't leave either unspecified.

Good:
```
Short press cycles 8 feedback stages from one repeat to full oscillation.
Long press toggles a delay-synced harmonic tremolo.
```

Vague:
```
The footswitch changes modes.
```

Common footswitch patterns from SpiralCaster's session:
- **Cycle through modes** — short press cycles through N options (with LED color per mode)
- **Toggle between two states** — short press toggles A ↔ B (LED tracks state)
- **Tap tempo** — short press taps delay time
- **Freeze/hold** — short press freezes capture or holds current state
- **Latch** — long press latches a state until long-pressed again
- **Reset** — long press resets to default and clears buffers
- **No-op** — explicitly say "long press does nothing" rather than leaving it implied

---

## Specify expression pedal explicitly

Expression is optional — if you don't mention it, the pipeline may wire it to the Right knob
(repo default) or leave it disconnected. Name the target:

```
Expression pedal controls HPF cutoff.
Expression sweeps Pitch and Tape Time across full ranges simultaneously.
Expression controls delay feedback (0.25–0.9).
```

If you don't want expression pedal at all, say so:
```
No expression pedal assignment needed.
```

---

## Name LED states for multi-mode effects

For effects with multiple modes or toggle states, tell the pipeline what color signals what:

```
LED blue when melt is on, dim white when off.
LED cyan for ducking mode.
Short press cycles LFO shapes with distinct LED colors per mode.
```

Without explicit LED instructions, colors are guessed. The pipeline generally uses blue for
"active" and dim white or dim blue for "off" — but anything with 3+ modes needs explicit
color guidance or you'll get whatever the pipeline picks.

---

## Describe signal flow in order

List processing stages in the order they should run:

```
BBD-style delay → massive distortion → harmonic tremolo → safety limiter
```

or:

```
Octave-up granular delay feeding into a massive stereo plate reverb feeding into a tremolo.
```

"Feeding into" and "running through" both work. The pipeline is reasonably good at parsing
serial chains from natural language. Parallel paths need more care:

```
Three independent delay lines running in parallel, each panned L/C/R.
A parallel forward tape delay and reverse plate reverb with linked timing.
```

---

## Use "decision prompts" for architectural choices

When the pipeline can't determine a fundamental parameter from your description, it may ask
a clarifying question before generating code. This is normal for ambiguous designs.

SpiralCaster's Moody Harp session:
```
(decision prompt)
One key detail decides the whole build: should the arpeggiator be
(1) MIDI-note in → generates a new note sequence,
(2) audio in → uses an internal synth voice, or
(3) audio in → chops/gates the input at arp timing?

(response)
2
```

You can pre-empt decision prompts by including the architectural choice in your initial
prompt. For arpeggiators, synthesizers, or anything that could be signal-processing vs.
sound-generation: state it explicitly.

---

## Start simple, then revise

The pipeline handles multi-round sessions well. A pattern that worked repeatedly in
SpiralCaster's session:

1. **First prompt:** describe the core concept + knob assignment
2. **First revision:** fix the one thing that wasn't right
3. **Second revision:** polish an edge case

Examples from the session:

BOTTO-WAH:
```
(prompt) create a bit crushed auto wah
(revision) Increase robotic quality of the bit crush. Make more of the range of the
"crush" control produce an audible effect.
```

GRAVITY MELT:
```
(revision) reproduce this exactly but with clicks and pops smoothed out.
```

RIPTIDE (two rounds of click-smoothing):
```
(revision 1) smooth the transient sounds of the reverse reverb peaks
(revision 2) eliminate or smooth out the clicks even further
```

Keep revisions focused — one thing per revision. "Fix the bias control AND change the LED
colors AND add a second mode" may produce mixed results. The pipeline handles targeted
changes more reliably than broad multi-change requests.

---

## Patterns that produce reliable results

These patterns consistently produced working effects across SpiralCaster's session:

**Gate on input level:**
```
A hard volume gate silences the effect when input drops below threshold.
```

**Freeze/hold:**
```
Short press toggles Freeze capture.
Long press clears the cloud with a quick fade then resumes capture.
```

**Tap tempo:**
```
Short press tap-tempo sets delay time.
```

**Near-oscillation feedback:**
```
Fixed high feedback always on the brink of oscillation.
Long press toggles feedback to maximum.
```

**Latched mode:**
```
Long press toggles a latched max-feedback mode until long pressed again.
```

**Expression sweeping two params simultaneously:**
```
Expression sweeps Pitch + Tape Time across full ranges.
```

---

## What to include in your initial prompt

For a standard effect, cover all of these:

```
[Effect concept — one sentence describing the core sound]

[Signal flow — list stages in order]

Knob 1 controls [parameter] from [min value/description] to [max value/description].
Knob 2 controls [parameter] from [min value/description] to [max value/description].
Knob 3 controls [parameter] from [min value/description] to [max value/description].

Short press [does what].
Long press [does what].
Expression pedal controls [parameter].
```

Omit sections that genuinely don't apply. For a simple overdrive you might not need an
expression pedal section. For a static texture generator you might not need footswitch
detail. But anything you leave out is something the pipeline decides for you.

---

## Token costs and complexity

From the reference section in `docs/endless-reference.md`:

| Effect complexity | Approximate cost |
|---|---|
| Simple (single algorithm) | ~$1 |
| Complex (multi-stage, multi-mode) | ~$2–5 |

Each revision round costs additional tokens. Three-round sessions like RIPTIDE and
GRAVITY MELT cost roughly 2–3× a single-shot effect. Budget accordingly if you're
iterating heavily on complex effects.

The 2000 bundled tokens ($20 value) that come with the pedal cover roughly 10–20 simple
effects or 5–10 complex multi-revision sessions.

---

## See also

- [`CATALOG.md`](examples/SpiralCaster_Examples/CATALOG.md) — 23 example effects with prompt-to-result logs
- [`docs/playground-to-sdk.md`](../docs/playground-to-sdk.md) — how to re-implement a Playground effect as an SDK patch
- [`docs/endless-reference.md §2`](../docs/endless-reference.md) — Playground overview and token economy
