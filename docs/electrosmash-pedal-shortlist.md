# Electrosmash Pedal Survey for Future Endless Patches

Reviewed on 2026-04-08 against the ElectroSmash pedals index and the linked pedal-analysis
pages:

- Pedals index: <https://www.electrosmash.com/homepage/pedals.html>

This document surveys every pedal listing on that page, then narrows the field to the
best candidates for future Polyend Endless patches in this repository.

The goal is not "most historically accurate emulation at any cost." The goal is to find
pedals that:

- have enough public circuit detail to build confidently
- fit the Endless control surface well
- can be implemented with stock-SDK-friendly DSP
- add something new to this repo rather than duplicating what is already here

Process note for future implementation turns: send an `ntfy` notification after every
completed build attempt, not just at the end of the whole task.

---

## Endless Selection Criteria

Each candidate is judged on five axes:

1. **Circuit clarity:** does ElectroSmash explain the circuit blocks and sound signature well?
2. **Control-surface fit:** can the effect make sense with 3 knobs, one expression target, and one press/hold footswitch pair?
3. **DSP fit:** can it be implemented cleanly with the stock SDK and the repo's current approach?
4. **Risk:** is the sound heavily dependent on analog quirks, rare parts, or interactions that are hard to carry into a first-pass digital patch?
5. **Repository value:** does it add a new effect family or a significantly stronger example than what already exists here?

Repository context matters:

- `chorus.cpp` already covers a chorus family effect
- `wah.cpp` already covers the Crybaby/Vox wah family
- `mxr_distortion_plus.cpp` already covers the Distortion+ family

That means a great ElectroSmash article is not enough by itself; the pedal also has to be
worth adding to this fork.

---

## Full Pedal Survey

### Back Talk Reverse Delay

Source: <https://electrosmash.com/back-talk-analysis>

- **ElectroSmash takeaway:** pure digital reverse delay with 3 knobs: `Mix`, `Speed`, `Repetitions`
- **Endless fit:** excellent; the original control set already matches the Endless surface
- **DSP fit:** excellent; reverse windowing/delay is more natural to do digitally than many analog pedals are to emulate
- **Risk:** moderate; buffer/window artifacts and feedback feel will need tuning, but the architecture is straightforward
- **Repository value:** realized in `back_talk_reverse_delay.cpp` as a chunk-based reverse delay with feedback, a hold-toggle texture mode, and equal-power expression-as-mix
- **Status:** `implemented`

### Big Muff Pi

Source: <https://www.electrosmash.com/big-muff-pi-analysis>

- **ElectroSmash takeaway:** 4 cascaded transistor stages, double clipping stages, passive tone stack, sustain/volume behavior well explained
- **Endless fit:** strong, but this repo now adapts the third control to `Blend` so expression stays performance-friendly
- **DSP fit:** strong; cascaded gain/clip/filter blocks are already within the repo's circuit-to-DSP style
- **Risk:** moderate; getting the "wall of sustain" and the Big Muff tone stack feeling right matters more than the raw topology
- **Repository value:** now realized in `big_muff.cpp` as a Ram's Head-inspired build with a Tone Bypass alternate voice
- **Status:** `implemented`

### Boss CE-2 Chorus

Source: <https://www.electrosmash.com/boss-ce-2-analysis>

- **ElectroSmash takeaway:** classic BBD chorus with strong circuit detail and guitar-oriented voicing
- **Endless fit:** good
- **DSP fit:** good; delay-line modulation is natural on Endless
- **Risk:** low to moderate
- **Repository value:** low right now because this repo already has `chorus.cpp`
- **Status:** `already covered`

### Boss DS-1 Distortion

Source: <https://www.electrosmash.com/boss-ds1-analysis>

- **ElectroSmash takeaway:** transistor booster into op-amp hard clipping with a Big Muff-style mid-scooped tone section
- **Endless fit:** very good; `Distortion`, `Tone`, `Level` map naturally
- **DSP fit:** very good; this is a practical stock-SDK distortion target
- **Risk:** moderate; the personality depends on the booster, hard clip, and tone stack interplay
- **Repository value:** good, but it overlaps with other 3-knob dirt pedals that may translate even more cleanly
- **Status:** `plausible`

### Dallas Rangemaster Treble Booster

Source: <https://electrosmash.com/dallas-rangemaster>

- **ElectroSmash takeaway:** one-transistor treble/upper-mid booster with strong emphasis on bias and frequency-shaping mods
- **Endless fit:** weak in stock form because the original pedal effectively has one main control
- **DSP fit:** good for a boost/voicing patch, but not as compelling as a first build
- **Risk:** moderate; a faithful version risks being too simple, while an expanded version becomes more reinterpretation than clone
- **Status:** `plausible`, but not recommended first

### Dunlop CryBaby GCB-95

Source: <https://www.electrosmash.com/crybaby-gcb-95>

- **ElectroSmash takeaway:** active filter plus inductor-based wah, frequency response and Vox differences documented
- **Endless fit:** excellent
- **DSP fit:** excellent; already proven in this repo's wah implementation
- **Risk:** low
- **Repository value:** already realized here in `wah.cpp`
- **Status:** `already covered`

### Fuzz Face

Source: <https://www.electrosmash.com/fuzz-face>

- **ElectroSmash takeaway:** 2-transistor germanium fuzz with strong emphasis on low input impedance, feedback, transistor gain, and bias
- **Endless fit:** only fair; a simple `Fuzz`, `Volume`, `Bias` layout is possible, but the real behavior depends on guitar interaction
- **DSP fit:** moderate
- **Risk:** high for a first-pass Endless patch; pickup loading, cleanup, and transistor-specific bias behavior are central to the sound
- **Status:** `not recommended first`

### Germanium Fuzz

Source: <https://www.electrosmash.com/germanium-fuzz>

- **ElectroSmash takeaway:** modernized Fuzz Face-style project with bias trimmers and careful transistor selection
- **Endless fit:** fair
- **DSP fit:** moderate
- **Risk:** high; the article itself reinforces how much the sound depends on transistor spread, leakage, and bias tuning
- **Repository value:** interesting as a later fuzz study, not a clean first target
- **Status:** `not recommended first`

### Ibanez Tube Screamer

Source: <https://electrosmash.com/tube-screamer-analysis>

- **ElectroSmash takeaway:** classic 3-knob overdrive with frequency-selective clipping, active bass rolloff in the clipping loop, post-clip tone shaping, and a clear mid-hump signature
- **Endless fit:** excellent; `Drive`, `Tone`, `Level` are already the right control set
- **DSP fit:** excellent; the repo already has the building blocks to approximate the high-pass-in-feedback feel, soft clipping, and tone shaping
- **Risk:** low to moderate; the sound signature is strong and well documented
- **Repository value:** now realized in `tube_screamer.cpp` as a TS808-grounded build with a TS9 alternate voice
- **Status:** `implemented`

### Klon Centaur

Source: <https://www.electrosmash.com/klon-centaur-analysis>

- **ElectroSmash takeaway:** transparent overdrive with buffered topology, germanium clipping, summing stage, and a mix of clean/dirty behavior
- **Endless fit:** good on paper with `Gain`, `Tone`, `Output`
- **DSP fit:** moderate to good
- **Risk:** moderate to high; the "transparent but pushed" feel depends on multiple interacting stages and clean/dirty blending
- **Repository value:** now realized in `klon_centaur.cpp` as a stock voice plus Tone Mod family variant
- **Status:** `implemented`

### MXR MicroAmp

Source: <https://www.electrosmash.com/mxr-microamp/pedals/clean/mxr-microamp-analysis.html>

- **ElectroSmash takeaway:** clean boost with transparent response and one main gain control
- **Endless fit:** weak in stock form; this wants one knob more than three
- **DSP fit:** trivial
- **Risk:** low
- **Repository value:** limited unless combined with extra voicing or utility features
- **Status:** `plausible`, but not recommended first

### Marshall Guv'nor

Source: <https://www.electrosmash.com/marshall-guvnor-analysis>

- **ElectroSmash takeaway:** Marshall-in-a-box distortion with gain and a fuller tone-control section
- **Endless fit:** mixed; the original pedal has more musically important controls than the Endless surface comfortably exposes
- **DSP fit:** good
- **Risk:** moderate; compressing the tone stack down to 3 knobs would require design choices that matter a lot to the result
- **Repository value:** good, but not as clean a fit as Rat or Tube Screamer
- **Status:** `not recommended first`

### MXR Distortion+

Source: <https://www.electrosmash.com/mxr-distortion-plus-analysis>

- **ElectroSmash takeaway:** simple op-amp plus germanium clipper with gain-coupled frequency behavior
- **Endless fit:** only fair in stock form because the original pedal has only two controls and a problematic analog pot law for Endless
- **DSP fit:** good; already demonstrated here
- **Risk:** already known and already addressed in this repo
- **Repository value:** already covered in `mxr_distortion_plus.cpp`
- **Status:** `already covered`

### MXR Dyna Comp

Source: <https://www.electrosmash.com/mxr-dyna-comp-analysis>

- **ElectroSmash takeaway:** OTA compressor with envelope detector, output stage, and characteristic coloration
- **Endless fit:** good if reinterpreted as `Sensitivity`, `Output`, and perhaps `Attack` or `Blend`
- **DSP fit:** moderate; dynamics work is feasible, but this repo does not yet have a compressor foundation
- **Risk:** moderate; a convincing compressor needs careful envelope tuning and output management
- **Repository value:** high as a later dynamics patch
- **Status:** `plausible`

### MXR Phase 90

Source: <https://www.electrosmash.com/mxr-phase-90/pedals/delay/mxr-phase-90.html>

- **ElectroSmash takeaway:** phase-shift blocks, moving notches, FET-as-variable-resistor behavior, and LFO section are well explained
- **Endless fit:** good if interpreted as `Rate`, `Depth/Feedback`, `Mix`
- **DSP fit:** moderate to good; digital phasing is straightforward, but the circuit's exact all-pass behavior and feedback flavor deserve care
- **Risk:** moderate
- **Repository value:** now realized in `phase_90.cpp` as a one-knob block/script Phase 90 build
- **Status:** `implemented`

### ProCo Rat

Source: <https://www.electrosmash.com/proco-rat-analysis/pedals/distortion/pro-co-rat-distortion.html>

- **ElectroSmash takeaway:** op-amp clipper with distinctive low/high-pass filtering, LM308 identity, and the famous `Filter` control
- **Endless fit:** excellent; `Distortion`, `Filter`, `Volume` are a natural 3-knob mapping
- **DSP fit:** excellent; clear topology, strong identity, and manageable implementation scope
- **Risk:** low to moderate; the clip/filter interaction must feel right, but the architecture is clean
- **Status:** `shortlisted`

### Sitar Swami

Source: <https://www.electrosmash.com/sitar-swami-analysis>

- **ElectroSmash takeaway:** effectively an overdriven octaver with a continuous flanger-like component rather than a literal sitar simulator
- **Endless fit:** interesting but awkward
- **DSP fit:** moderate; could become a great creative patch, but only as a deliberate reinterpretation
- **Risk:** high for a first target because the sound category itself is fuzzy and hybrid
- **Status:** `not recommended first`

### Vox V847 Wah Wah

Source: <https://www.electrosmash.com/vox-v847-analysis>

- **ElectroSmash takeaway:** classic wah with sweep range, Q mods, and resonant band-pass behavior clearly documented
- **Endless fit:** excellent
- **DSP fit:** excellent; already reflected in this repo's wah work
- **Risk:** low
- **Repository value:** already covered in `wah.cpp`
- **Status:** `already covered`

### EMG81 Pickup

Source: <https://www.electrosmash.com/emg81>

- **ElectroSmash takeaway:** useful electronics article, but it is not a pedal
- **Endless fit:** not a patch target in the same sense as the pedal analyses
- **Status:** `excluded`

---

## Original Top Four — Status Update

These were the four ElectroSmash pedal listings that translated best into worthwhile Endless
patches for this repository. Three of the four are now implemented.

### 1. Tube Screamer — `implemented`

Why it made the cut:

- classic 3-knob control layout already fits Endless
- circuit blocks are clearly explained by ElectroSmash
- frequency-selective clipping gives it more identity than a generic overdrive
- stock-SDK-friendly first implementation path is obvious

Realized as `tube_screamer.cpp`: TS808 voice with TS9 hold-toggle, expression-on-tone.

### 2. ProCo Rat — `shortlisted`

Why it makes the cut:

- clean 3-knob mapping
- very strong identity from clipper plus `Filter` behavior
- slightly more aggressive/distinct than Tube Screamer
- simpler control story than Guv'nor or Klon

Main design opportunity:

- build a Rat-inspired distortion with careful `Filter` taper and output management

**This is the only remaining shortlisted ElectroSmash candidate without an implementation.**

### 3. Big Muff Pi — `implemented`

Why it made the cut:

- iconic fuzz/sustain family with a direct 3-knob mapping
- ElectroSmash explains the stage structure and tone control well
- adds a true fuzz/sustain voice that this repo does not yet have

Realized as `big_muff.cpp`: Ram's Head-inspired fuzz, Tone Bypass hold-toggle, expression-as-blend.

### 4. Back Talk Reverse Delay — `implemented`

Why it made the cut:

- strongest non-drive candidate
- original pedal is already fundamentally digital, which plays to Endless strengths
- 3 knobs already match Endless expectations
- highly creative without requiring analog black-magic translation

Realized as `back_talk_reverse_delay.cpp`: chunk-based reverse delay with feedback damping,
hold-toggle texture mode, and equal-power expression-as-mix.

---

## Near Misses

These are good articles and plausible future patches, but they missed the original top four for this repo.

### Klon Centaur

- very interesting, but harder than Tube Screamer to get "right" because the appeal is in subtle clean/dirty interaction
- now `implemented` as `klon_centaur.cpp` with a stock voice and a Tone Mod alternate

### MXR Phase 90

- strong future candidate, but a second modulation-family patch was lower priority than adding Rat, Big Muff, or reverse delay
- now `implemented` as `phase_90.cpp` as a one-knob block/script Phase 90

### MXR Dyna Comp

- strong future dynamics candidate, but compressor behavior is more tuning-sensitive than the shortlisted options

### Boss DS-1

- absolutely buildable, but Rat and Tube Screamer give a cleaner pair of complementary dirt targets first

### Dallas Rangemaster

- historically important, but too simple in stock form to be the best use of the Endless interface

### MXR MicroAmp

- useful utility effect, but too minimal to be the best next showcase patch

---

## Lower-Priority For First Implementation

### Marshall Guv'nor

- too many important original controls for a comfortable first-pass Endless translation

### Fuzz Face

- too dependent on pickup loading, bias, and transistor behavior to be a clean first fuzz implementation

### Germanium Fuzz

- same core issue as Fuzz Face, with even more emphasis on transistor matching and bias trimming

### Sitar Swami

- creatively appealing, but better approached later as a deliberate reinterpretation rather than a direct first build

---

## Remaining Build Priority

Of the original top four, only **ProCo Rat** remains unimplemented. The next best
candidates from the full survey are:

1. **ProCo Rat** — still the strongest remaining ElectroSmash target: clean `Distortion /
   Filter / Volume` mapping, very strong sound identity, manageable implementation scope.
2. **MXR Dyna Comp** — best dynamics candidate; `Sensitivity / Output` with a third control
   for `Attack` or `Blend` gives a good 3-knob story once the repo has one dynamics example.
3. **Boss DS-1** — absolutely buildable as a harder-edged complement to the Tube Screamer;
   lower priority now that the drive family is well represented.

---

## Practical Recommendation

The repo now has strong coverage of overdrive, fuzz, phaser, chorus, wah, and reverse delay.
The clearest gap is distortion with a distinctive `Filter`-style taper and dynamics processing.

If you want the **next distinct dirt voice**, build **ProCo Rat** next.

If you want the **first dynamics patch**, build **MXR Dyna Comp** next.
