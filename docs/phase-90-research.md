# Phase 90 Research Notes for Endless

Reviewed for this fork on 2026-04-08.

Primary source:

- ElectroSmash MXR Phase 90 Analysis: <https://www.electrosmash.com/mxr-phase90>

Supporting script-mod context:

- Analog Man Phase 90 / script mod notes: <https://www.analogman.com/mxr.htm>

---

## What ElectroSmash Establishes Clearly

The ElectroSmash article gives a clean block model for the Phase 90:

1. input buffer
2. four-stage phase shifting network
3. LFO
4. output mixer
5. optional feedback emphasis in later block-logo units

The most important takeaways for an Endless patch are:

- the effect is a four-stage phaser, not a chorus or flanger
- the notches are created by recombining dry signal with a phase-shifted path
- the audible sweep is driven by an LFO moving the all-pass network
- the block vs script distinction is strongly tied to the added feedback resistor behavior

That makes the script-mod decision unusually clean for this repo: the alternate voice should be smoother because the feedback is removed or reduced, not because the patch invents a second phaser effect.

---

## What Matters for Endless

The original Phase 90 is famously a one-knob pedal. This build should keep that identity.

Chosen control story:

- Mid knob = `Speed`
- hold = block / script toggle
- expression mirrors `Speed`
- Left and Right knobs remain intentionally unused as public controls

That is the right compromise for Endless. It keeps the patch honest to the hardware while still allowing expression performance control because of the repo’s hardwired param-2 expression routing.

---

## Script vs Block Voice

The audible distinction that matters most here:

- **Block voice:** stronger, more pronounced, more chewy because of the feedback path
- **Script voice:** smoother, softer, more balanced, less emphasized in the mids

The implementation should preserve that relationship. The script mode should not become a separate modern preset; it should simply feel like the vintage, de-feedbacked voice.

---

## Potentiometer and Control-Law Lessons

The original pedal has only one public potentiometer, so the parameter work is simple but still important.

Main risks:

- a speed range that bunches all useful movement into one part of the knob
- an overly fast upper end that stops sounding like a classic Phase 90
- a lower end so slow that it reads as static

So the Endless build should:

- use a log-style speed mapping
- choose a moderate default that sounds immediately familiar
- keep expression matched to the same speed law instead of inventing a second mapping

This build also introduces a new documentation lesson for the repo: a patch can intentionally leave controls unused when preserving the original hardware is the right design choice.

---

## What This Patch Is and Is Not

What it is:

- a classic one-knob Phase 90-inspired phaser
- a block voice plus script-mod alternate
- a linked-stereo implementation of a fundamentally mono-style effect

What it is not:

- a stereo-enhanced phaser with offset LFOs
- a modern multi-control phaser
- a patch that uses all three knobs just because the pedal surface happens to provide them

That boundary matters. The value of this build is the discipline to keep the UI simple while still making the DSP and voicing feel authentic.
