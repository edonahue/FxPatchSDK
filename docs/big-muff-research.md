# Big Muff Pi Research Notes for Endless

Reviewed for this fork on 2026-04-08.

Primary source:

- ElectroSmash Big Muff Pi Analysis: <https://www.electrosmash.com/big-muff-pi-analysis>

Supporting variant context:

- Kit Rae Big Muff history and variant guide: <https://www.kitrae.net/music/big_muff_historyB.html>

---

## What ElectroSmash Establishes Clearly

The ElectroSmash analysis gives a clean block-level model of the classic Big Muff Pi:

1. input booster
2. two cascaded clipping stages
3. passive tone control
4. output booster

The most important takeaways for a digital Endless patch are:

- the Big Muff is not a single clipping stage; the stacked clipping stages are central to the sound
- the clipping stages are deliberately bandwidth-limited, which smooths the fuzz and extends sustain
- the classic tone control is a passive low-pass/high-pass blend that creates the familiar mid scoop near the midpoint
- the analog pedal uses three 100K linear potentiometers: `Sustain`, `Tone`, and `Volume`

Those facts are enough to build a behaviorally faithful patch without pretending to emulate every transistor bias point.

---

## Variant Context That Matters Here

The Big Muff family is broad, but not every variant difference matters equally for a first Endless build.

Useful high-level distinctions:

- `Triangle`: earlier, more open, slightly less compressed reputation
- `Ram's Head`: smoother and singing, often preferred for lead sustain
- `Green Russian`: thicker and less scooped, often read as lower-mid heavier
- `Op-Amp`: more aggressive and less transistor-voiced than the classic transistor family

For this repository, the best first target is a **Ram's Head-inspired** voice because it preserves the iconic sustained lead character while staying close to the ElectroSmash transistor block model.

---

## Endless-Specific Design Constraints

This fork exposes:

- 3 parameters
- 1 global expression target on param `2`
- 1 footswitch press action
- 1 footswitch hold action

That makes a literal `Sustain` / `Tone` / `Volume` mapping less attractive than it is on the hardware pedal, because param `2` should usually remain a live-performance control. For this patch:

- Left = `Sustain`
- Mid = `Tone`
- Right / Expression = `Blend`

That is a deliberate Endless adaptation. It preserves the two musically defining Muff controls and turns the third control into a live-performance blend rather than a static output trim.

---

## Chosen Voice Strategy

Primary mode:

- `Ram's Head`-inspired Muff voice
- classic LP/HP tone-stack blend
- scooped and singing at the center of the tone control

Hold-toggle alternate mode:

- `Tone Bypass`-inspired mids-lift voice
- removes the classic scoop and replaces it with a flatter, louder post-clip voicing
- keeps the `Tone` knob active as a gentler top-end control rather than disabling the middle knob entirely

This is intentionally not a full multi-variant emulator. It is one strong Muff voice plus one musically useful alternate mode that the Endless footswitch can expose cleanly.

---

## Potentiometer and Control-Law Lessons

The analog pedal uses three linear pots. That does not mean the Endless patch should use literal linear internal mappings.

The main risks are:

- `Sustain` bunching too much distortion near the top of the sweep
- `Tone` only sounding useful at the extreme ends
- `Blend` causing a loudness cliff when expression moves toward full wet

For this patch, the right design priorities are:

- use an eased or power-law `Sustain` mapping
- keep the tone-stack sweep bounded and guitar-relevant
- use an equal-power or compensated `Blend` mapping so heel-to-toe expression stays musical

---

## What This Patch Is and Is Not

What it is:

- a circuit-informed Big Muff interpretation for Endless
- grounded in the ElectroSmash block analysis
- voiced toward Ram's Head sustain and lead usefulness
- adapted to the repo's expression routing and footswitch model

What it is not:

- a transistor-by-transistor SPICE recreation
- a full catalog of every historical Big Muff variant
- a literal 3-knob clone with output volume on the expression-controlled knob

That tradeoff is intentional. The repo is trying to build strong, playable Endless patches, not only schematic transcriptions.
