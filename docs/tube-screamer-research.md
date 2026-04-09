# Tube Screamer Research Notes for Endless

Reviewed for this fork on 2026-04-08.

Primary source:

- ElectroSmash Tube Screamer Analysis: <https://electrosmash.com/tube-screamer-analysis>

Supporting family context:

- Analog Man Tube Screamer history and variants: <https://www.analogman.com/tshist.htm>

---

## What ElectroSmash Establishes Clearly

The ElectroSmash Tube Screamer analysis gives a strong block-level model for an Endless patch:

1. bass-controlled input shaping
2. op-amp overdrive with frequency-selective clipping
3. post-clip tone control
4. output level stage

The most important implementation takeaways are:

- the clipping is not a generic full-band distortion stage
- the overdrive character comes from emphasizing the upper mids before clipping while trimming bass
- the resulting sound is the familiar Tube Screamer mid hump rather than a flat overdrive response
- the original control story is already ideal for Endless: `Drive`, `Tone`, `Level`

This makes Tube Screamer one of the cleanest ElectroSmash circuits to translate into this repo.

---

## Variant Context That Matters Here

For this build, the useful family distinction is narrow rather than broad:

- `TS808`: the primary baseline and the closest match to the ElectroSmash reference
- `TS9`: a very close family relative with a slightly brighter, tighter reputation
- `TS10`: a valid future option, but a bigger departure from the ElectroSmash baseline than this first build needs

That leads to a simple Endless strategy:

- default voice = `TS808`
- hold-toggle alternate voice = `TS9`

This keeps the alternate mode within the same family instead of turning one patch into a grab bag of unrelated overdrives.

---

## Endless-Specific Design Constraints

This repo still hardcodes expression to param `2`. For Tube Screamer, that creates a different tradeoff than Big Muff:

- the classic `Drive` / `Tone` / `Level` layout is already excellent
- changing the third control to `Blend` would make the patch less Tube Screamer-like
- keeping `Tone` on param `2` preserves the original control story, but expression must then sweep brightness in a musical way

So the chosen Endless mapping is:

- Left = `Drive`
- Mid = `Level`
- Right / Expression = `Tone`

That is the most faithful and least awkward adaptation for this circuit.

---

## Potentiometer and Control-Law Lessons

The original pedal uses the right control set, but not every analog pot law should be copied directly into normalized Endless knob space.

Main risks:

- `Drive` bunching most of the audible overdrive near the upper end
- `Level` becoming too hot when `Drive` is high
- `Tone` feeling dull through most of the sweep, then getting sharp only near the top
- expression on `Tone` becoming too extreme at heel or toe

So the first-pass Endless design should:

- use an eased `Drive` mapping
- compensate output level against drive so the middle knob stays usable
- keep the `Tone` sweep bounded to a guitar-appropriate range
- verify that the full heel-to-toe tone sweep remains performance-safe

---

## What This Patch Is and Is Not

What it is:

- a TS808-grounded Tube Screamer family patch
- a circuit-informed translation of the ElectroSmash analysis
- a close TS808 / TS9 family switch rather than a broad multi-overdrive emulator

What it is not:

- a component-for-component emulation of every resistor and capacitor tolerance
- a patch that replaces the classic Tube Screamer third knob with a non-classic Endless-only control
- a generic soft-clipping overdrive with a Tube Screamer label

That boundary matters. The value of this build is that the circuit and the control story already fit Endless well, so the implementation should stay disciplined.
