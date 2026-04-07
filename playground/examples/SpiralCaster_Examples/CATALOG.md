# SpiralCaster Effect Catalog

23 `.endl` patches from YouTube creator [SpiralCaster](https://www.youtube.com/@SpiralCaster),
demonstrated in [The MOST DANGEROUS Pedal I've Ever Used: Polyend ENDLESS](https://youtu.be/nCYzbPEIBZo?si=Ehx_ZuWXTwMUoEj0).

Each entry shows the final effect description from the PDF prompt log.
Knobs are Left / Mid / Right.

---

## AUTOSYNTH PSYCHONAUT
**Category:** Synth · **Arch type:** Autopad Phaser

Autonomous minor-chord pad synth (replaces input) running through an 8-stage phaser into a
high-resonance ladder LPF, with soft limiting and input-envelope-assisted cutoff motion.

| Knob | Parameter | Range |
|---|---|---|
| Left | Key | 0–11 (chromatic root) |
| Mid | PhDepth | 0–1 |
| Right | Filter | 0–1 |

**Short press:** cycles 8 phaser speed stages with distinct LED colors  
**Long press:** toggles melt octave-down glide (LED blue when on)  
**Expression:** controls Filter cutoff (smoothed brightening post-phaser)

---

## BELLOWS
**Category:** Reverb · **Arch type:** PlateGate

Stereo plate reverb with input-level-detected dynamics. Default mode gates the reverb tail
when input drops; toggle switches to ducking mode where reverb swells as signal drops.

| Knob | Parameter | Range |
|---|---|---|
| Left | SizeDec | 0–1 (size/decay macro) |
| Mid | Thresh | 0–1 (gate/duck threshold) |
| Right | Tone | 0–1 (tail brightness) |

**Short press:** toggles Gated ↔ Ducking (LED cyan for Ducking)  
**Long press:** resets to Gated and flushes reverb state

---

## BOTTO-WAH
**Category:** Modulation · **Arch type:** Synth Sweep

LFO-modulated wah filter with ZOH sample-rate reduction + bit quantization (robotic crush),
with click-free PRE/POST routing blend.

| Knob | Parameter | Range |
|---|---|---|
| Left | Rate | 0.05–8.0 Hz |
| Mid | WahRange | 200–4000 Hz |
| Right | Crush | 1–16 steps |

**Short press:** cycles LFO shape (triangle → sine → square)  
**Long press:** toggles pre-crush vs post-crush routing

---

## CASCADE FLANGE
**Category:** Delay / Modulation · **Arch type:** Fast Cascade Flange

Wet-only pitched cascade flange built on an ultra-fast modulated delay with feedback near
self-oscillation. Pitch shifts repeats to specific intervallic targets. Hard gate silences
everything when input drops. Optional parallel depth delay adds dimension.

| Knob | Parameter | Range |
|---|---|---|
| Left | Interval | −12..+12 semitones (chromatic steps) |
| Mid | Rate | 0.1–20 Hz (flange rate) |
| Right | HPF | 20–6000 Hz |

**Short press:** cycles 4 flange modes (subtle → moderate → intense → extreme metallic)  
**Long press:** toggles parallel slow depth delay  
**Expression:** controls HPF cutoff in real time

---

## CLOUDSTRETCH
**Category:** Granular · **Arch type:** Granular Stretcher

Captures recent audio into a ring buffer and overlaps windowed grains for a smooth pad-like
wash. Speed control tilts post-filter and character; reverse/slow settings are dark and
submerged, fast settings shimmer.

| Knob | Parameter | Range |
|---|---|---|
| Left | GrainSize | 0.01–0.6 s |
| Mid | Density | 0.0–1.0 |
| Right | Speed | −2.0..+2.0 (center snaps to 1.0 = normal) |

**Short press:** toggles Freeze (halts capture, loops current grain cloud)  
**Long press:** clears cloud with quick fade then resumes capture

---

## DROPGATE
**Category:** Glitch · **Arch type:** Dropout Gate

Stereo dropout gate with random drop scheduling (glitchy) or deterministic chop (tremolo).
Gain envelopes are smoothed by the Character control.

| Knob | Parameter | Range |
|---|---|---|
| Left | DropRate | 0–1 (~0.2–20 Hz) |
| Mid | DropSize | 0–1 (5–300 ms in Random / 5–95% duty in Chop) |
| Right | Char | 0–1 (smooth → choppy edges) |

**Short press:** toggles Random ↔ Chop  
**Expression:** controls DropSize

---

## EVENT HORIZON
**Category:** Reverb · **Arch type:** Infinite Plate

Massive stereo plate reverb with high diffusion and wide stereo decorrelation. Sustain knob
has an infinite hold region — new notes layer continuously on top of the held tail, accumulating
into an ever-thickening wash.

| Knob | Parameter | Range |
|---|---|---|
| Left | Size | 0–1 |
| Mid | Sustain | 0–1 (infinite hold region ≥ 0.90) |
| Right | Tone | 0–1 |

**Short press:** releases hold and briefly damps the tail  
**Long press:** toggles hold latch (LED cyan when latched)

---

## FAULT LINE
**Category:** Glitch · **Arch type:** Hard Stutter

Hard-cut stutter glitch modeled after Akai Headrush E2 tape loop glitching. Records into a
stereo ring buffer and periodically replaces live input with a repeated fragment with no
smoothing at boundaries.

| Knob | Parameter | Range |
|---|---|---|
| Left | FragSize | 5–1000 ms |
| Mid | Random | 0–1 (fixed and predictable → fully randomized) |
| Right | Rate | 0–1 (occasional one-shots → rapid machine-gun) |

**Short press:** toggles forced glitch hold (LED dark lime)  
**Long press:** toggles Freeze record head (LED dark cobalt)

---

## GHOST THEREMIN
**Category:** Synth · **Arch type:** SynthTapeDual

Clean sine oscillator into tape-style Delay A then a subtly modulated Delay B, with a
transparent output limiter. Expression sweeps both Pitch and Tape Time simultaneously.

| Knob | Parameter | Range |
|---|---|---|
| Left | Pitch | 30–1760 Hz |
| Mid | TapeTime | 20–900 ms |
| Right | Delay FB | 0–100% |

**Short press:** toggles post-Delay B chop tremolo  
**Long press:** toggles latched max-feedback mode for Delay B (dark red LED when latched)  
**Expression:** sweeps Pitch + Tape Time across full ranges

---

## GRAVITY MELT
**Category:** Delay · **Arch type:** Pitch-Descending Delay

Stereo feedback delay where each repeat begins +12 st above the source note and continuously
glides down to −12 st over its lifespan. Multiple repeats at higher feedback settings create
a cascading stack of voices simultaneously melting at different pitch positions.

| Knob | Parameter | Range |
|---|---|---|
| Left | MeltSpd | 0.25–12.0 s (glacial to fast descent) |
| Mid | DelayTime | 30–900 ms |
| Right | Feedback | 0.0–0.95 |

**Short press:** tap tempo for delay time  
**Long press:** toggles freeze (high feedback)

---

## JACOB'S LADDER
**Category:** Filter · **Arch type:** Ladder Wah

Moog-style 4-pole ladder LPF with fixed soft drive. Manual mode uses expression to sweep
cutoff logarithmically between the Bottom and Top Hz knob values. LFO mode adds fast
modulation with selectable waveforms.

| Knob | Parameter | Range |
|---|---|---|
| Left | BottomHz | 20–5000 Hz (expression heel position) |
| Mid | TopHz | 200–20000 Hz (expression toe position) |
| Right | Reson | 0.0–1.0 |

**Short press:** engages LFO and cycles waveforms (Triangle → Sine → Square → Ramp → S&H)
at fast fixed tiers (8 / 16 / 32 / 64 / 128 Hz)  
**Long press:** returns to Manual mode, resets LFO phase/S&H  
**Expression:** sweeps cutoff log-scale between BottomHz and TopHz

---

## MOODY HARP
**Category:** Synth · **Arch type:** MoodyArpDelay

Internal minor arpeggiator synth voice (no input required) running into a clean digital delay.
Runs continuously regardless of input signal.

| Knob | Parameter | Range |
|---|---|---|
| Left | Key | −12..+12 semitones |
| Mid | ArpRate | 1–12 Hz |
| Right | DlyTime | 60–600 ms |

**Short press:** cycles arp direction (up / down / up-down / down-up / random)  
**Long press:** toggles high-feedback mode  
**Expression:** controls delay feedback (0.25–0.9, clamped ≥ 0.8 in high mode)

---

## OILY BOY
**Category:** Delay · **Arch type:** Oil Can Delay

Triple oil-can delay: Delay 1 is a fixed hot slapback near self-oscillation with oil-can
saturation + lowpass in the feedback path; Delays 2 and 3 are user-timed and summed
wet-only, followed by a HPF on all wet signal.

| Knob | Parameter | Range |
|---|---|---|
| Left | D2 Time | 20–1200 ms |
| Mid | D3 Time | 20–1200 ms |
| Right | Feedback | 0.0–0.98 (shared; curves to near-unity for all three lines) |

**Short press:** cycles Delay 1 time modes (6 / 12 / 24 / 60 ms)  
**Long press:** toggles freeze sustain without cutting input  
**Expression:** sweeps wet HPF cutoff (40–2000 Hz)

---

## ORBITAL WASHER
**Category:** Reverb · **Arch type:** Plate-Phasor

Massive stereo plate reverb (100% wet) feeding directly into a strong stereo phaser in
series, creating a hypnotic rotating ambience — "like standing inside a vast reverberant
room that is slowly spinning."

| Knob | Parameter | Range |
|---|---|---|
| Left | Size | 0–1 |
| Mid | DecayInf | 0–1 (long natural decay to infinite sustain) |
| Right | PhSpeed | 0–1 (glacial to animated phaser sweep) |

**Short press:** toggles freeze/hold of the reverb input  
**Long press:** fast wash reset (tail clear) while processing continues

---

## OUROBOROS
**Category:** Modulation · **Arch type:** Stereo Phaser

Stereo multi-stage phaser with wide L/R phase-offset LFO and fixed wet blend. Long press
triggers a momentary swell that boosts resonance and stereo width, then smoothly returns.

| Knob | Parameter | Range |
|---|---|---|
| Left | Rate | 0.02–8.0 Hz |
| Mid | Reson | 0–95% |
| Right | Stages | 2 / 4 / 6 / 8 / 10 / 12 (stepped) |

**Short press:** cycles LFO waveform (sine → triangle → saw up → saw down → square → S&H)
with distinct LED color per waveform  
**Long press:** triggers momentary resonance/width swell then returns  
**Expression:** controls Rate

---

## REVERSE MICRO
**Category:** Granular / Reverse · **Arch type:** Reverse Micro-Sampler

Continuously captures input into a circular buffer and plays a moving reverse window with
10 ms crossfades at loop boundaries. Freeze locks the current window into a perpetual
reverse loop while live playing continues underneath.

| Knob | Parameter | Range |
|---|---|---|
| Left | PreDelay | 0.0–1.5 s |
| Mid | Speed | 0.25–2.0× (always reverse) |
| Right | SampSize | 0.02–4.0 s |

**Short press:** toggles Freeze (LED white = off / blue = frozen)  
**Expression:** controls playback speed in real time

---

## RIPTIDE
**Category:** Reverb / Delay · **Arch type:** Dual Reverse Plate

Parallel tape delay and reverse plate reverb with linked timing so the reverse swell peaks
exactly as the delay repeat begins to fade. Click-free reverse via windowed/crossfaded
segments with DC blocking.

| Knob | Parameter | Range |
|---|---|---|
| Left | Time | 0.05–1.50 s (linked across both effects) |
| Mid | RevDec | 0.0–1.0 (reverse reverb decay) |
| Right | Feedbck | 0.0–0.92 (tape delay feedback) |

**Short press:** alternates toggling delay then reverb with smooth fades  
**Long press:** clears delay/reverse buffers

---

## SHOEGAZI
**Category:** Delay / Distortion · **Arch type:** BBD Wall

HPF-conditioned BBD-style echo feeding massive distortion, followed by a delay-synced
harmonic tremolo (band-split) and final safety limiter — a shoegaze wall-of-sound machine.

| Knob | Parameter | Range |
|---|---|---|
| Left | Delay | 20–800 ms |
| Mid | Drive | 0–30 dB |
| Right | HPF | 20–1200 Hz |

**Short press:** cycles 8 feedback stages (one repeat → near oscillation)  
**Long press:** toggles delay-synced harmonic tremolo on/off  
**Expression:** controls HPF cutoff

---

## SILICON SUPER VILLAIN
**Category:** Distortion · **Arch type:** FY-2 Fuzz

Shin-ei FY-2 Companion silicon transistor fuzz model with bias-starved gating, two-stage
clipping, and a resonant mid-scoop notch that gets more nasal/formant-like as Scoop
increases.

| Knob | Parameter | Range |
|---|---|---|
| Left | Bias | 0–1 (starved/sputtering → fully biased) |
| Mid | Fuzz | 0–2 (light saturation → chainsaw compression) |
| Right | Scoop | 0–1 (deep scooped hollow → flat with body) |

**Short press:** toggles hi-gain mode  
**Long press:** cycles clip topology (3 modes: hard / softer asymmetric / …)

---

## STARDUST
**Category:** Granular / Reverb · **Arch type:** Shimmer-Plate

Three-processor serial chain: octave-up granular delay → massive always-long stereo plate
reverb → post-tremolo with soft limiting. The plate decay is fixed at long/open — no decay
control, always enormous.

| Knob | Parameter | Range |
|---|---|---|
| Left | Delay | 20–900 ms |
| Mid | Grain | 5–350 ms |
| Right | TremRate | 0.08–12 Hz |

**Short press:** tap tempo (very fast double-tap cycles tremolo depth modes: subtle → medium
→ high → full chop)  
**Long press:** clear/reset granular + reverb wash  
**Expression:** controls tremolo rate in real time

---

## STUTTERFRAG
**Category:** Glitch · **Arch type:** Forward/Reverse Looper

Continuously captures audio and alternates playback between forward and reverse directions
with crossfaded direction flips. Balance knob biases relative time in each direction.

| Knob | Parameter | Range |
|---|---|---|
| Left | Fragment | 5–400 ms |
| Mid | FlipRate | 0.5–25 Hz |
| Right | Balance | −1..+1 (−1 = all reverse, 0 = equal, +1 = all forward) |

**Short press:** toggles Freeze (halts recording)  
**Long press:** re-syncs to recent audio, restarts on a forward segment with short fade-in

---

## TRIPLE SLAPPER
**Category:** Delay · **Arch type:** Triple Slapback

Three parallel slapback delay lines panned L/C/R with independent staggered time ranges.
HPF in the feedback path progressively thins repeats without touching the dry signal.

| Knob | Parameter | Range |
|---|---|---|
| Left | Feedback | 0.0–0.95 (shared across all three lines) |
| Mid | Time | 0.0–1.0 (sweeps A: 30–90 ms / B: 50–120 ms / C: 70–200 ms) |
| Right | RepeatHPF | 0–5000 Hz |

**Long press:** toggles feedback slam to max and back  
*(Short press: no-op)*

---

## WALL PUSHER
**Category:** Reverb / Distortion · **Arch type:** Distorted Plate

Massive wet-only stereo plate reverb feeding directly into aggressive distortion — the
reverb tail itself is saturated rather than the dry signal. Transparent safety limiter at
output prevents dangerous levels at high drive.

| Knob | Parameter | Range |
|---|---|---|
| Left | Size/Decay | 0–1 |
| Mid | Drive | 0–1 |
| Right | Tone | 0–1 (dark/warm → bright/searing) |

**Short press:** cycles clipping modes (hard → soft → asymmetric)  
**Long press:** resets to hard clip and clears limiter state

---

## Quick Reference Table

| Effect | Category | Left | Mid | Right | Expr |
|---|---|---|---|---|---|
| AUTOSYNTH PSYCHONAUT | Synth | Key 0–11 | PhDepth | Filter | Filter |
| BELLOWS | Reverb | SizeDec | Thresh | Tone | — |
| BOTTO-WAH | Modulation | Rate | WahRange | Crush | — |
| CASCADE FLANGE | Delay/Mod | Interval ±12st | Rate | HPF | HPF |
| CLOUDSTRETCH | Granular | GrainSize | Density | Speed | — |
| DROPGATE | Glitch | DropRate | DropSize | Char | DropSize |
| EVENT HORIZON | Reverb | Size | Sustain | Tone | — |
| FAULT LINE | Glitch | FragSize | Random | Rate | — |
| GHOST THEREMIN | Synth | Pitch | TapeTime | Delay FB | Pitch+Time |
| GRAVITY MELT | Delay | MeltSpd | DelayTime | Feedback | — |
| JACOB'S LADDER | Filter | BottomHz | TopHz | Reson | Cutoff |
| MOODY HARP | Synth | Key | ArpRate | DlyTime | Dly FB |
| OILY BOY | Delay | D2 Time | D3 Time | Feedback | Wet HPF |
| ORBITAL WASHER | Reverb | Size | DecayInf | PhSpeed | — |
| OUROBOROS | Modulation | Rate | Reson | Stages | Rate |
| REVERSE MICRO | Reverse | PreDelay | Speed | SampSize | Speed |
| RIPTIDE | Reverb/Delay | Time | RevDec | Feedbck | — |
| SHOEGAZI | Delay/Dist | Delay | Drive | HPF | HPF |
| SILICON SUPER VILLAIN | Distortion | Bias | Fuzz | Scoop | — |
| STARDUST | Granular | Delay | Grain | TremRate | TremRate |
| STUTTERFRAG | Glitch | Fragment | FlipRate | Balance | — |
| TRIPLE SLAPPER | Delay | Feedback | Time | RepeatHPF | — |
| WALL PUSHER | Reverb/Dist | Size/Decay | Drive | Tone | — |
