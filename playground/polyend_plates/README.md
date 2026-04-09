# Polyend Plates Archive

This directory documents the official [Polyend Plates](https://polyend.com/plates/)
collection for Endless and keeps a small tracked sample of official `.endl`
binaries in the repo.

As of 2026-04-08, the live Plates catalog exposed 45 product pages with direct
`Download free effect` links. This fork now supports two archive layers:

- a small tracked sample of five official plate binaries kept in git as reference artifacts
- a larger local-only sync of the full current catalog, downloaded from Polyend product pages and intentionally kept out of git by local exclude rules

## Attribution and Use

- All plate names, binaries, product descriptions, demo media, product pages, and related branding in this archive are attributed to Polyend.
- This fork does not claim authorship, ownership, relicensing rights, or redistribution rights for those official plate materials.
- The local `.endl` downloads in this directory were obtained from Polyend's own product pages and `Download free effect` links.
- Polyend's Terms state Online Shop content should be used "solely for one's own personal purposes." This fork follows that conservative reading by keeping the full synced archive local-only and committing documentation plus only a limited sample of existing reference binaries.
- A few live product pages currently omit an explicit `By:` line, but the collection itself is still an official Polyend catalog and is attributed here to Polyend as the publisher and download source.
- For the official source of truth, use the linked Polyend product pages directly.

## Local Sync Workflow

Use the repo helper script to sync the current live catalog into this directory
without adding the new binaries to git:

```bash
bash scripts/sync_polyend_plates.sh --dry-run
bash scripts/sync_polyend_plates.sh
bash scripts/sync_polyend_plates.sh --manifest
```

The repo keeps `playground/polyend_plates/*.endl` excluded locally via
`.git/info/exclude`, so a full local sync stays off the public branch by default.
The five already-tracked sample binaries remain tracked unless they are
explicitly changed.

## Tracked Sample Binaries

- `Dual-Lines.endl`
- `Malleus_Fuzz.endl`
- `Slapper_Morton.endl`
- `Wax.endl`
- `Yield_reverse_delay.endl`

## Current Catalog

| Local file | Plate | Categories | Author | Status | Description | Source |
| --- | --- | --- | --- | --- | --- | --- |
| `65-Sparkle.endl` | [65 Sparkle](https://polyend.com/product/65-sparkle/) | Drive, Sim | Polyend | downloaded locally only | Blackface-era amp simulation with tube-like sag, bright-cap bite, a musical tilt tone control, and a post-amp cut. | [Product page](https://polyend.com/product/65-sparkle/) |
| `808.endl` | [808](https://polyend.com/product/808/) | Drum Machine, Other, Utility | Polyend | downloaded locally only | A self-contained 808-style drum machine with a 16-step sequencer that plays without any input signal to generate kick, snare, and hi-hats, with light saturation and dynamics for punch. | [Product page](https://polyend.com/product/808/) |
| `Alt-Drums.endl` | [Alt Drums](https://polyend.com/product/alt-drums/) | Drum Machine, Utility | Polyend | downloaded locally only | Self-playing drum generator with an art-rock/IDM feel: off-kilter grooves, punchy kicks, snappy snares, airy hats. | [Product page](https://polyend.com/product/alt-drums/) |
| `Amp-Studio.endl` | [Amp Studio](https://polyend.com/product/amp-studio/) | Drive, Sim | Polyend | downloaded locally only | A mid-1960s deluxe American combo amp emulation, from clean sheen to edge-of-breakup and into warm crunch with more drive. | [Product page](https://polyend.com/product/amp-studio/) |
| `Arp.endl` | [ARP](https://polyend.com/product/arp/) | Other, Pitch | Polyend | downloaded locally only | Arp is an effect that converts an incoming signal into an arpeggiated melody. | [Product page](https://polyend.com/product/arp/) |
| `Bounce.endl` | [Bounce](https://polyend.com/product/bounce/) | Delay | Polyend | downloaded locally only | Bounce is an effect that uses ball physics as the backbone of the delay engine. | [Product page](https://polyend.com/product/bounce/) |
| `Brawl.endl` | [Brawl](https://polyend.com/product/brawl/) | Drive | Polyend | downloaded locally only | Our take on a multi-mode high-gain distortion designed to deliver a sound of stacked British Hi-Gain amps, for those who want a thick and saturated sound ideal for hard rock, metal, and lead tones. | [Product page](https://polyend.com/product/brawl/) |
| `chords.endl` | [Chords](https://polyend.com/product/chords/) | Experimental, Other, Utility | Polyend | downloaded locally only | A self-playing chord synth that runs through preset progressions while you play along. | [Product page](https://polyend.com/product/chords/) |
| `Doom.endl` | [Doom](https://polyend.com/product/doom/) | Drive | Polyend | downloaded locally only | Gritty, fuzzy....fuzz! With a noise gate designed to cut noise without chopping a guitar's decay, these gated fuzz tones are designed for breezy stoner riffs, not overdriven metal chugging. | [Product page](https://polyend.com/product/doom/) |
| `drift.endl` | [Drift](https://polyend.com/product/drift/) | Drive, Modulation, Reverb | Polyend | downloaded locally only | Shoegaze-style chain that stacks lush reverb, flanger motion, and fuzz into one patch. | [Product page](https://polyend.com/product/drift/) |
| `Dual-Lines.endl` | [Dual Lines](https://polyend.com/product/dual-lines/) | Delay, Drive | Polyend | tracked in repo | Dual delay where two lines are tied together instead of running side by side: a phasered ping-pong and a gritty tape echo. | [Product page](https://polyend.com/product/dual-lines/) |
| `Endless-Delay.endl` | [Endless Delay](https://polyend.com/product/endless-delay/) | Delay | Polyend | downloaded locally only | A freezable delay that can repeat endlessly without breakup. | [Product page](https://polyend.com/product/endless-delay/) |
| `Glitch-Loop.endl` | [Glitch Loop](https://polyend.com/product/glitch-loop/) | Glitch, Looper | Polyend | downloaded locally only | Looper that slices the recorded audio into equal parts and reshuffles them, creating a nice glitch even on one chord. | [Product page](https://polyend.com/product/glitch-loop/) |
| `Glitch-Swarm.endl` | [Glitch Swarm](https://polyend.com/product/glitch-swarm/) | Glitch | Polyend | downloaded locally only | A noisy, messy, chaotic, repeat machine and novel effect that uses a novel and unique approach to creating audio disruptions. | [Product page](https://polyend.com/product/glitch-swarm/) |
| `Glow.endl` | [Glow](https://polyend.com/product/glow/) | Pitch, Reverb | Polyend | downloaded locally only | Glow is a lush shimmer reverb that turns your notes into a wide, luminous space with a smooth octave bloom on top. | [Product page](https://polyend.com/product/glow/) |
| `GritFold-Phaser.endl` | [GritFold Phaser](https://polyend.com/product/gritfold-phaser/) | Modulation | Polyend | downloaded locally only | A slow, chewy phaser with grit and a little "warped tape" motion. | [Product page](https://polyend.com/product/gritfold-phaser/) |
| `Grunt-Octaver.endl` | [Grunt](https://polyend.com/product/grunt/) | Glitch, Pitch | Polyend | downloaded locally only | A lo-fi, glitchy analog-style octaver that's built for single notes and dirty stacks, not clean polyphonic chords. | [Product page](https://polyend.com/product/grunt/) |
| `Hold-Your-Horses_OD.endl` | [Hold Your Horses](https://polyend.com/product/hold-your-horses/) | Drive | Polyend | downloaded locally only | A dynamic, multi-mode overdrive that lets you crank up your amplifier. | [Product page](https://polyend.com/product/hold-your-horses/) |
| `Infinity-Hall.endl` | [Infinite Hall](https://polyend.com/product/infinite-hall/) | Reverb | Polyend | downloaded locally only | A cavernous, glossy hall that blooms wide with a silky, expansive tail and a gentle living swirl. | [Product page](https://polyend.com/product/infinite-hall/) |
| `Jangler.endl` | [Jangler](https://polyend.com/product/jangler/) | Drive, Sim | Polyend | downloaded locally only | Simulation of a classic 70s British tube amp with a tight low-end, glassy top, and mid-range bite going from clean to breakup. | [Product page](https://polyend.com/product/jangler/) |
| `Malleus_Fuzz.endl` | [Malleus](https://polyend.com/product/malleus/) | Drive | Polyend | tracked in repo | A thick fuzz with big sustain and real control over the output. | [Product page](https://polyend.com/product/malleus/) |
| `Memory-Cloud.endl` | [Memory Cloud](https://polyend.com/product/memory-cloud/) | Reverb | Polyend | downloaded locally only | Memory Cloud is an airy, pad-like diffusion reverb that turns notes into a gentle, enveloping cloud. | [Product page](https://polyend.com/product/memory-cloud/) |
| `Mutant-Spiral.endl` | [Mutant Spiral](https://polyend.com/product/mutant-spiral/) | Drive, Modulation, Utility | Polyend | downloaded locally only | A Mutator-style 24 dB/oct low-pass filter with SSM-style saturation. | [Product page](https://polyend.com/product/mutant-spiral/) |
| `Nebula.endl` | [Nebula](https://polyend.com/product/nebula/) | Granular | Polyend | downloaded locally only | A granular texture effect built for drifting, smeared ambience, and reversing tails. | [Product page](https://polyend.com/product/nebula/) |
| `Pattern-Tremolo.endl` | [Pattern Tremolo](https://polyend.com/product/pattern-tremolo/) | Modulation | Polyend | downloaded locally only | A stereo, full-wet patterned tremolo made for sustained notes. | [Product page](https://polyend.com/product/pattern-tremolo/) |
| `Prism-Trails.endl` | [Prism Trails](https://polyend.com/product/prism-trails/) | Delay, Reverb | Polyend | downloaded locally only | Spectral-ducking delay/reverb that carves around your dry notes so a cinematic wash blooms without masking the attack. | [Product page](https://polyend.com/product/prism-trails/) |
| `Rats-Screamer.endl` | [RatScreamer](https://polyend.com/product/ratscreamer/) | Drive | Polyend | downloaded locally only | We took the most extreme (and ridiculous) versions of a Rat and Tube Screamer possible and blended them in parallel for a dual-drive, parallel distortion. | [Product page](https://polyend.com/product/ratscreamer/) |
| `Rhythm.endl` | [Rhythm](https://polyend.com/product/rhythm/) | Drum Machine, Utility | Polyend | downloaded locally only | A TR-606-style drum machine that generates its own kick/snare/hat groove. | [Product page](https://polyend.com/product/rhythm/) |
| `Saturator.endl` | [Saturator](https://polyend.com/product/saturator/) | Drive, Sim | Polyend | downloaded locally only | Smooth tape compression with pre/de-emphasis, musically asymmetric bias, and optional low-end "head bump." Full-wet with no added gain staging. | [Product page](https://polyend.com/product/saturator/) |
| `Slapper_Morton.endl` | [Slapper Morton](https://polyend.com/product/slapper-morton/) | Drive | Polyend | tracked in repo | Aggressive drive with a tight slapback delay on top. | [Product page](https://polyend.com/product/slapper-morton/) |
| `Softwash.endl` | [Softwash](https://polyend.com/product/softwash/) | Modulation, Reverb | Polyend | downloaded locally only | Soft, ambient diffusion that turns guitar notes into a weightless, evolving wash. | [Product page](https://polyend.com/product/softwash/) |
| `Sound-Cutter.endl` | [Sound Cutter](https://polyend.com/product/sound-cutter/) | Experimental, Glitch, Looper | Polyend | downloaded locally only | Performance slicer that buffers a rolling second of audio, then freezes and chops it into an instant cut-up loop when triggered. | [Product page](https://polyend.com/product/sound-cutter/) |
| `Spectral-Delay.endl` | [Spectral Delay](https://polyend.com/product/spectral-delay/) | Delay, Experimental | Polyend | downloaded locally only | Spectral delay is an experimental delay that breaks up your signal into different frequency bands and sends them to multiple delay lines. | [Product page](https://polyend.com/product/spectral-delay/) |
| `Spirals.endl` | [Spirals](https://polyend.com/product/spirals/) | Delay | Polyend | downloaded locally only | Spirals goes beyond a standard golden ratio delay with a recursive feedback mode for crossfeeding delay lines to create beautiful, unique delay spirals. | [Product page](https://polyend.com/product/spirals/) |
| `Splice.endl` | [Splice](https://polyend.com/product/splice/) | Looper, Sim | Polyend | downloaded locally only | A tape-style looper built for pitch-shifted loops. | [Product page](https://polyend.com/product/splice/) |
| `Sunset.endl` | [Sunset](https://polyend.com/product/sunset/) | Reverb | Polyend | downloaded locally only | Smooth plate-or-room reverb with musical ducking and stereo drift so wide pads bloom when you stop playing. | [Product page](https://polyend.com/product/sunset/) |
| `Synthesist.endl` | [Synthesist](https://polyend.com/product/synthesist/) | Other | Polyend | downloaded locally only | Self-generating chord progression synth with no input required, built around evolving four-chord pads and spacious reverb. | [Product page](https://polyend.com/product/synthesist/) |
| `Tape-Aging.endl` | [Tape Aging](https://polyend.com/product/tape-aging/) | Modulation, Sim | Polyend | downloaded locally only | Tape machine effect that gradually accumulates wear, adding saturation, wobble, softened highs, and occasional ghosting over time. | [Product page](https://polyend.com/product/tape-aging/) |
| `Tape_Glue.endl` | [Tape Glue](https://polyend.com/product/tape-glue/) | Drive, Modulation, Sim | Polyend | downloaded locally only | Warm tape saturation paired with ping-pong slapback, flutter, azimuth smear, and feedback-aware ducking. | [Product page](https://polyend.com/product/tape-glue/) |
| `Tape-Loop.endl` | [Tape Looper](https://polyend.com/product/tape-looper/) | Looper | Polyend | downloaded locally only | A lo-fi tape-style looper that captures a phrase and replays it with flexible length, speed, and vintage color. | [Product page](https://polyend.com/product/tape-looper/) |
| `Tape-Scanner.endl` | [Tape Scanner](https://polyend.com/product/tape-scanner/) | Delay, Sim | Polyend | downloaded locally only | A gritty, lo-fi stereo tape delay inspired by the legendary tape echo. | [Product page](https://polyend.com/product/tape-scanner/) |
| `Tessera.endl` | [Tessera](https://polyend.com/product/tessera/) | Granular, Pitch, Reverb | Polyend | downloaded locally only | A granular arpeggiator that can bloom into a stereo ambient pad. | [Product page](https://polyend.com/product/tessera/) |
| `Vintage-Cab.endl` | [Vintage Cab](https://polyend.com/product/vintage-cab/) | Sim, Utility | Polyend | downloaded locally only | A cabinet simulator that makes your DI feel like a mic'd cab with four models. | [Product page](https://polyend.com/product/vintage-cab/) |
| `Wax.endl` | [Wax](https://polyend.com/product/wax/) | Modulation, Sim | Polyend | tracked in repo | A vinyl-character modulation effect that feels like warped wax and hazy memory, with multiple vinyl simulation modes. | [Product page](https://polyend.com/product/wax/) |
| `Yield_reverse_delay.endl` | [Yield](https://polyend.com/product/yield/) | Delay | Polyend | tracked in repo | A lo-fi reverse delay built for soft, "tape-like" smears and swirling back-spun phrases. | [Product page](https://polyend.com/product/yield/) |

## Notes

- Descriptions above are brief paraphrases from official Polyend product pages, not reverse-engineered claims about the underlying DSP.
- Local inspection with `strings` is still useful for spot-checking embedded labels, but the product pages remain the authoritative source for naming, categories, behavior, and downloads.
- The broader local archive is meant for personal Endless-owner reference, listening comparison, naming/category research, and documentation context inside this fork.
