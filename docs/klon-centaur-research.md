# Klon Centaur Research Notes for Endless

Reviewed for this fork on 2026-04-08.

Primary source:

- ElectroSmash Klon Centaur Analysis: <https://www.electrosmash.com/klon-centaur-analysis>

Supporting mod context:

- Coda Effects Klon Centaur mods and tweaks: <https://www.coda-effects.com/p/klon-centaur-mods-and-tweaks.html>

---

## What ElectroSmash Establishes Clearly

The ElectroSmash Klon analysis gives a strong block-level model for an Endless patch:

1. input buffer
2. op-amp gain stage with germanium clipping
3. summing stage
4. active tone control
5. output stage

The most important implementation takeaways are:

- the Klon is not just a clipped path; its identity depends on summing the clipped path with cleaner feed-forward content
- the gain control changes more than distortion amount, because the clean/dirty relationship shifts with it
- the active treble shelving EQ is part of the sound, not just a generic low-pass tone knob
- the pedal is famous for its boost-to-overdrive range, not for extreme saturation

Those are the behaviors the Endless patch needs to preserve.

---

## What Matters for Endless

This repo hardcodes expression to param `2`, so the control layout decision matters a lot.

For Klon, the right call is to preserve the classic three-control story:

- Left = `Gain`
- Mid = `Treble`
- Right / Expression = `Output`

Unlike the Big Muff build, repurposing the third knob to `Blend` would weaken the actual Klon identity. The output control is also a musically strong expression target because the Klon is commonly used as a boost.

---

## Main Design Risk

Klon is harder than Tube Screamer because the appeal is subtle:

- too little clean-path contribution and it becomes a generic overdrive
- too much clean-path contribution and it stops feeling like a pushed Klon
- too much gain range and it drifts into distortion-pedal territory
- too wide a treble sweep and the active shelf becomes harsh instead of useful

That means the most important parameter review item is not maximum gain. It is whether the patch keeps the **clean/dirty interaction** believable across the full gain sweep.

---

## Tone Mod Alternate Voice

The selected alternate voice is a **Tone Mod** inspired by the common C14-style Klon mod.

Useful implementation consequence:

- lower the shelving corner somewhat
- keep more fullness in the response
- make the treble control feel less thin at comparable settings

This is a better Endless alternate mode than a “hot” gain mode because it keeps the patch inside the Klon family.

---

## Potentiometer and Control-Law Lessons

The original Klon control set already fits Endless, but the pot behavior still needs adaptation.

Main risks:

- `Gain` doing too little in the first half, then suddenly sounding clipped
- `Treble` becoming a narrow bright-only control
- `Output` becoming too hot once gain is raised
- expression on `Output` producing a loudness cliff instead of a usable boost sweep

So the Endless build should:

- use an eased `Gain` mapping
- tie the gain control to internal clean/dirty rebalance, not only to clip drive
- keep the `Treble` shelf bounded and centered on a guitar-appropriate range
- taper `Output` for live boost use and compensate enough to stay controllable

---

## What This Patch Is and Is Not

What it is:

- a Centaur-grounded transparent overdrive
- a clean/dirty summing overdrive with active treble control
- a stock-voice plus Tone Mod family variant

What it is not:

- a literal power-supply simulation of the original 27 V headroom arrangement
- a KTR-first patch
- a high-gain distortion pedal wearing a Klon label

That boundary is important. The Klon build should be judged by whether it stays open, stackable, and dynamically useful, not by how much saturation it can produce.
