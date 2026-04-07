# SpiralCaster Playground Examples

23 `.endl` patches from YouTube creator [SpiralCaster](https://www.youtube.com/@SpiralCaster),
demonstrated in [The MOST DANGEROUS Pedal I've Ever Used: Polyend ENDLESS](https://youtu.be/nCYzbPEIBZo?si=Ehx_ZuWXTwMUoEj0).

Each effect folder contains:
- `.endl` — ready-to-load binary; drag onto the Endless drive via USB-C
- `.pdf` — full prompt log showing the original prompt, any revisions, and the final result description

**Quick reference for all controls:** [`CATALOG.md`](CATALOG.md)  
**Prompting patterns from this session:** [`../../prompting-best-practices.md`](../../prompting-best-practices.md)

---

## Effects at a glance

| Effect | Type | Notable feature |
|---|---|---|
| [AUTOSYNTH PSYCHONAUT](AUTOSYNTH%20PSYCHONAUT/) | Synth pad | Autonomous minor-chord pad; 8-stage phaser; octave melt |
| [BELLOWS](BELLOWS/) | Reverb | Stereo plate; gated tails ↔ ducking swells toggle |
| [BOTTO-WAH](BOTTO-WAH/) | Modulation | LFO wah + ZOH bitcrush; PRE/POST routing |
| [CASCADE FLANGE](CASCADE%20FLANGE/) | Delay/Mod | Pitched feedback cascade; hard gate; 4 flange modes |
| [CloudStretch](CloudStretch/) | Granular | Granular stretcher; freeze capture; speed-tilted texture |
| [DropGate](DropGate/) | Glitch | Random dropout or deterministic chop tremolo |
| [EVENT HORIZON](EVENT%20HORIZON/) | Reverb | Infinite sustain plate; accumulating ambient wash |
| [FAULT LINE](FAULT%20LINE/) | Glitch | Hard-cut stutter; modeled on Akai Headrush E2 tape glitch |
| [Ghost Theremin](Ghost%20Theremin/) | Synth | Sine osc → dual tape delays; latched max-feedback mode |
| [GRAVITY MELT](GRAVITY%20MELT/) | Delay | Each repeat glides +12 → −12 st; tap tempo; freeze |
| [Jacob's Ladder](Jacob%27s%20Ladder/) | Filter | 4-pole Moog ladder; expression-swept log cutoff; LFO |
| [Moody Harp](Moody%20Harp/) | Synth | Internal minor arpeggiator + digital delay |
| [Oily Boy](Oily%20Boy/) | Delay | Triple oil-can delay; D1 near self-oscillation |
| [Orbital Washer](Orbital%20Washer/) | Reverb | Infinite plate → stereo phaser; rotating ambience |
| [Ouroboros](Ouroboros/) | Modulation | Multi-stage stereo phaser; 6 LFO waveforms; LED per mode |
| [Reverse Micro](Reverse%20Micro/) | Reverse | Continuous reverse micro-sampler; freeze loop |
| [RIPTIDE](RIPTIDE/) | Reverb/Delay | Tape delay + reverse plate; linked timing handoff |
| [SHOEGAZI](SHOEGAZI/) | Delay/Dist | BBD delay → massive distortion; shoegaze wall of sound |
| [Silicon Super Villain](Silicon%20Super%20Villain/) | Distortion | FY-2 silicon fuzz; bias-starved gating; resonant mid-scoop |
| [Stardust](Stardust/) | Granular | Octave-up granular → infinite plate → tremolo chain |
| [StutterFrag](StutterFrag/) | Glitch | Forward/reverse alternating looper; crossfaded direction flips |
| [TRIPLE SLAPPER](TRIPLE%20SLAPPER/) | Delay | Three slapback lines L/C/R; staggered times; HPF on repeats |
| [WALL PUSHER](WALL%20PUSHER/) | Reverb/Dist | Stereo plate → distortion; reverb tail saturated, not dry signal |

---

## Session notes

SpiralCaster's session shows several patterns worth studying:

**Multi-round refinement is normal.** Of the 23 effects, roughly half went through at least
one revision. RIPTIDE needed two rounds of click-smoothing on the reverse reverb; GRAVITY MELT
needed one round of transient cleanup; SILICON SUPER VILLAIN needed three revisions to get
the bias control and scoop character right.

**Targeted revision requests work best.** Compare GRAVITY MELT's effective revision
("reproduce this exactly but with clicks and pops smoothed out") to a hypothetical
"fix everything" — focused, one-thing requests produce reliable results.

**Explicit knob specs in the prompt produce matching knob specs in the result.** Every effect
where SpiralCaster named knobs explicitly got exactly those knob assignments back. The few
terse prompts (BOTTO-WAH: "create a bit crushed auto wah") got sensible defaults, then
needed a revision to tune the behavior.

**Decision prompts signal an architectural fork.** When Playground asked SpiralCaster to
choose between three arpeggiator architectures, that was the system identifying an ambiguity
it couldn't resolve from the prompt alone. You can pre-empt these by being explicit about
whether your effect is signal-processing or sound-generation.

See [`../../prompting-best-practices.md`](../../prompting-best-practices.md) for the full
patterns guide extracted from this session.
