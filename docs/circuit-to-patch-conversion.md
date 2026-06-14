# Circuit-to-Patch Conversion: Methodology Guide

This document captures the methodology for converting analog guitar effect circuits into
Polyend Endless SDK patches. It is written for future development sessions and human
developers who want to replicate or design new effects based on real circuit analysis.

---

## Overview of the Workflow

```
1. Identify circuit blocks
2. Map blocks to DSP primitives
3. Determine control mappings
4. Address gain staging
5. Implement with SDK constraints
6. Validate with tests/check_patches.sh
```

---

## Step 1: Identify Circuit Blocks

Start from the schematic. Every analog circuit can be decomposed into a small set of
functional blocks:

| Analog block | Common components | What it does |
|---|---|---|
| High-pass filter | Series capacitor + resistor | Removes DC and low frequencies |
| Low-pass filter | Shunt capacitor + resistor | Removes high frequencies |
| Band-pass / notch | LC or RC networks | Shapes midrange |
| Amplifier (gain) | Op-amp (non-inverting or inverting) | Boosts signal level |
| Clipping / saturation | Diodes, BJT, JFET, tube | Introduces harmonic distortion |
| Reverb / delay | Spring, bucket-brigade (BBD), tape | Time-domain effects |
| Modulation | LFO + VCA or BBD | Chorus, flanger, tremolo |
| Compression | VCA + side-chain | Dynamics control |

For each block, note: the component values, what controls them (fixed vs. potentiometer),
and what other blocks they interact with.

---

## Step 2: Map Blocks to DSP Primitives

### RC High-Pass Filter → 1-Pole IIR HP

**Analog:** Series capacitor C, shunt resistor R.  
**Cutoff:** `fc = 1 / (2π × R × C)`  
**DSP (Euler-Backward / bilinear approximation):**

```cpp
// Coefficient (recompute when fc changes)
float alpha_hp = 1.0f / (1.0f + kTwoPi * fc / kSampleRate);

// Per-sample (maintain hpPrev and xPrev as state)
float hpOut = alpha_hp * (hpPrev + x - xPrev);
xPrev   = x;
hpPrev  = hpOut;
```

**Note:** `alpha_hp → 1` as fc → 0 (passes all); `alpha_hp → 0.5` as fc → fs/2 (approaches
all-pass at Nyquist). At typical audio frequencies (< 1 kHz), this 1-pole model is accurate.

### RC Low-Pass Filter → 1-Pole IIR LP

**Analog:** Series resistor R, shunt capacitor C.  
**Cutoff:** `fc = 1 / (2π × R × C)`  
**DSP:**

```cpp
// Coefficient
float omega   = kTwoPi * fc / kSampleRate;
float alpha_lp = omega / (1.0f + omega);

// Per-sample
float lpOut = alpha_lp * x + (1.0f - alpha_lp) * lpPrev;
lpPrev = lpOut;
```

**Note:** `alpha_lp → 0` as fc → 0 (blocks all); `alpha_lp → 1` as fc → fs (passes all).

### Multi-Pole Filters (2nd order and above)

For 2-pole (12 dB/oct) responses, cascade two 1-pole filters. For Biquad (2nd-order with
resonance peak), use the standard Biquad IIR formulation. Most guitar pedal tone stacks are
1-pole (6 dB/oct), so cascading simple 1-pole filters is usually sufficient.

### State-Variable Filter (SVF) → Resonant Bandpass / LP / HP

Use when you need a **resonant bandpass** with independently controllable center frequency and
Q — e.g., wah, envelope filter, formant filter. The Chamberlin SVF yields all three filter
outputs (lowpass, bandpass, highpass) from one pass through three equations.

**When to prefer SVF over biquad:**
- When frequency and Q need to be independently set (expression pedal frequency + Q knob)
- When code simplicity matters (3 lines/sample vs. 5-coefficient biquad)
- When you want all three filter outputs simultaneously

```cpp
// Coefficient (recompute when fc or Q changes — once per buffer)
constexpr float kPi = 3.14159265f;
float f1 = 2.0f * sinf(kPi * fc / kSampleRate);  // frequency coeff
float q1 = 1.0f / Q;                               // damping coeff (= 1/Q)

// Per-sample (maintain lo and band as state — initialized to 0)
lo   += f1 * band;
float hi = x - lo - q1 * band;
band += f1 * hi;

// Outputs:
//   lo   = lowpass  (rolls off above fc)
//   band = bandpass (peak at fc, gain = Q/2)
//   hi   = highpass (rolls off below fc)
```

**Gain normalization (bandpass):** SVF bandpass peak gain = Q/2. To normalize to unity:
```cpp
float wet = band * (2.0f * q1);  // = band * (2/Q); peak gain = 1.0 at all Q values
```

**Stability:** The Chamberlin SVF is stable when `f1² + 2×q1×f1 < 4`. In practice this
means fc < ~3 kHz at 48 kHz sample rate is safe for Q ≤ 10. For higher fc or Q, consider
the Cytomic/Andy Simper SVF formulation (more numerically robust but slightly more complex).

**WDF caveat for high-Q resonant sections.** A WDF-style circuit-faithful port of a
Sallen-Key or other high-Q filter section is *not* a drop-in upgrade over the SVF above.
The `sthompsonjr` fork shipped a 2026-04-30 fix replacing two WDF Sallen-Key filters
(corner near 2.1 kHz / 3.8 kHz) with bilinear-transform biquads after the originals
diverged within ~261 samples at 48 kHz. If you are tempted to model a wah, envelope
filter, or other resonant section as a WDF circuit, plan the fallback path before you
commit. See
[`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md) for the
incident detail and our open question about adding an impulse-response stability check
to the local probe.

**State clearing:** When fc changes abruptly (e.g., mode switch), clear `lo` and `band` to
zero to avoid a transient pop. When sweeping smoothly (expression pedal), do NOT clear —
the state continuity IS the sweep character.

### Amplifier Gain Stage → Scalar Multiply

**Non-inverting configuration:** `gain = 1 + R_feedback / R_input`  
**Inverting configuration:** `gain = -R_feedback / R_input` (invert sign)

```cpp
float gained = x * gain;
```

When gain is controlled by a potentiometer, recompute `gain` once per buffer, not per sample.

### Diode Clipping → Waveshaper

Diode clipping is nonlinear — the DSP equivalent is a waveshaper function applied to each sample.

The diode forward voltage (`Vf`) determines the knee sharpness. Lower `Vf` = softer onset =
lower `k` in the tanh model. Silicon diodes clip harder than germanium for the same circuit
topology; model this by increasing the `k` scaling factor.

| Clipping type | Component | Vf | DSP | k factor |
|---|---|---|---|---|
| Ultra-soft symmetric | Germanium (1N270, 1N34A) — e.g. MXR D+ | ~0.3 V | `tanhf(x)` | k = 1 |
| Soft symmetric | Vacuum tube triode | ~2–5 V (plate) | `tanhf(x)` or polynomial | k = 1–2 |
| Medium symmetric | Silicon (1N914, 1N4148) — e.g. DOD 250 | ~0.65 V | `tanhf(x * k)` | k = 2–4 |
| Hard symmetric | LED clipping | ~1.8–2.2 V | `fmaxf(-1, fminf(1, x))` | — |
| Asymmetric | Mixed diode types or single diode | varies | Asymmetric tanh: separate +/− paths | — |

**Standard germanium soft clip (k=1):**
```cpp
float clipped = tanhf(gained);  // always in (-1, +1); smooth onset
```

**Silicon harder clip (k≈3):**
```cpp
float clipped = tanhf(gained * 3.0f);  // sharper knee, closer to hard limiting
```

**Where local WDF-style work lives.** Diode-pair clipping is the lower-risk corner of
the WDF design space and is where this repo's two WDF-style sibling patches live:
[`effects/big_muff_wdf.cpp`](../effects/big_muff_wdf.cpp) and
[`effects/tube_screamer_wdf.cpp`](../effects/tube_screamer_wdf.cpp). If you are weighing
"hand-tuned `tanhf` versus a wave-solved diode pair" for a new drive patch, the existing
sibling-pair experiment is the cheapest precedent to read. See
[`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md) for
why diode-pair WDF is treated as opt-in rather than default in this repo.

**Cubic soft clip (faster than tanhf, valid only for |x| ≤ 1):**
```cpp
float clipped = x - (x * x * x) / 3.0f;
```

**Hard clip (LED, zener):**
```cpp
float clipped = fmaxf(-1.0f, fminf(1.0f, gained));
```

**Asymmetric (mixed diode types — one germanium, one silicon, or single-diode):**
```cpp
float clipped = (gained >= 0.0f)
    ? tanhf(gained)               // soft positive (germanium-like)
    : -tanhf(-gained * 3.0f);     // harder negative (silicon-like)
```

### Delay-Based Effects → Working Buffer

For reverb, chorus, flanger, echo: use the working buffer provided by the SDK.
See `source/Patch.h` for `kWorkingBufferSize = 2400000` floats (~9.6 MB).

```cpp
void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override {
    delayL_ = buf.data();          // partition the buffer
    delayR_ = buf.data() + kLen;
}
```

**Tape/BBD-flavored modeling — fork precedent.** If a patch wants the *character* of a
bucket-brigade delay (EH-7850 family, Deluxe Memory Man, etc.) rather than just a clean
delay line, the `sthompsonjr` fork's `wdf/DmmBbdCore.h` (BBD core), `wdf/DmmDelayCircuit.h`
(IIR feedback EQ + assembled topology), and `dsp/DmmCompander.h` (NE570 behavioral
compander) are the closest external worked example. We have not adopted them — license
status is unresolved and our existing
[`effects/back_talk_reverse_delay.cpp`](../effects/back_talk_reverse_delay.cpp) and
[`effects/chorus.cpp`](../effects/chorus.cpp) target a different feel — but the
companding-plus-feedback-EQ structure is the right idea-level reference. See
[`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md).

---

## Step 3: Determine Control Mappings

The Endless has three knobs (Left, Mid, Right) and one expression pedal.
`internal/PatchCppWrapper.cpp` in this repository currently routes the expression pedal
to param 2 (Right knob) for all patches.

Important distinction: current Polyend Endless device docs describe choosing the target
knob for expression in pedal setup. This repository's wrapper is narrower and currently
exposes only the Right-knob lane to patch code.

**Mapping strategy:**

1. Identify the 2–3 most musically important controls in the original circuit.
2. Assign them to knobs in decreasing order of importance (Left = most important).
3. If a parameter has a wide logarithmic range (e.g., frequency or gain), apply a log taper
   to make the knob feel even across its travel.
4. Reserve Right knob (param 2) for level or mix if you want expression pedal control.

### Control-Law Review Checklist

Treat knob mapping and knob taper as separate decisions.

A control can be "correct" in circuit terms and still feel wrong on Endless. The MXR
Distortion+ patch is the clearest example in this repo: the original analog gain law
stacked most of the audible change near the end of the knob travel, which felt like a
dead zone on hardware. When reviewing or designing a patch, answer these four questions
for every parameter:

1. **Audible sweep:** does the control do useful work across most of the knob travel?
2. **Taper choice:** should it be linear, logarithmic, exponential/power-law, or bounded?
3. **Default value:** does the default land on a musically usable sound, not just the numeric midpoint?
4. **Expression interaction:** if this is param `2`, does the same mapping still make sense from heel to toe?

Use the simplest mapping that matches the job:

- **Linear:** best for blend, mix, and narrow-range offsets
- **Log taper:** best for frequency, rate, and very wide ratio-based ranges
- **Power-law / eased curve:** useful when a linear knob feels front-loaded or back-loaded
- **Bounded / compensated mapping:** useful when one control changes both tone and loudness

### Current Patch Examples

- `chorus.cpp`: good example of a log taper for rate and linear mappings for depth and mix
- `wah.cpp`: good example of a log frequency sweep because the ear hears pitch movement geometrically
- `bbe_sonic_stomp.cpp`: uses bounded blend and delay-offset mappings rather than analog-pot emulation
- `mxr_distortion_plus.cpp`: main cautionary example; raw analog pot math was less usable than an Endless-tuned curve
- `big_muff.cpp`: shows how to adapt a classic `Volume` control into expression-friendly `Blend` while keeping the tone stack musical
- `tube_screamer.cpp`: shows when preserving the original `Drive` / `Level` / `Tone` control story matters more than remapping param `2`
- `klon_centaur.cpp`: shows how a gain control can rebalance clean and clipped paths instead of just increasing clip amount
- `phase_90.cpp`: shows how to keep a one-knob pedal authentic while leaving other controls intentionally unused

**Log taper example:**
```cpp
// Rate knob: 0.1 Hz at 0.0, 5 Hz at 1.0
float hz = 0.1f * powf(50.0f, rate_);
```

**Power-law / eased example:**
```cpp
// Distortion control: more audible change earlier in the sweep
float driveCurve = std::pow(dist_, 0.75f);
float gain = 1.5f + 16.0f * driveCurve;
```

**Frequency range mapping:**
```cpp
// Tone knob: 3 kHz at 0.0, 20 kHz at 1.0
float fc = 3000.0f + tone_ * 17000.0f;
```

**Level with output compensation:**
```cpp
// Keep a level control useful even when another parameter increases loudness
float levelCurve = level_ * (0.5f + 0.5f * level_);
float outputTrim = 1.15f - 0.45f * driveCurve;
float out = shaped * levelCurve * outputTrim;
```

### Passive Tone Stacks and Blend Compensation

Classic pedals such as the Big Muff use passive tone stacks that lose level while shaping the
response. That matters twice on Endless:

1. the tone control can sound "dead" if the shaped branches are not compensated enough
2. if param `2` becomes `Blend` for expression, the wet path cannot jump in level when the pedal
   goes toe-down

Design guidance:

- if a tone control is implemented as an LP/HP blend, check the midpoint carefully because that is
  usually where the strongest notch or scoop appears
- if a passive-style tone stage loses level, add makeup gain after the tone section, not only
  before it
- when replacing a literal analog `Volume` knob with `Blend`, prefer equal-power or compensated
  dry/wet mixing over a plain linear crossfade
- if an alternate mode bypasses the classic tone stack, keep the middle knob musically relevant
  unless you have a stronger reason to disable it

### Active Overdrive Tone and Expression

Some classic overdrives, especially the Tube Screamer family, already fit the Endless control
surface well. In those cases, forcing param `2` to become mix or output level can be the wrong
move if it breaks the original pedal story.

Design guidance:

- preserve `Drive` / `Tone` / `Level` when that control set is central to the pedal identity
- if expression must live on `Tone`, keep the sweep bounded enough for heel-to-toe use on guitar
- when drive and clipping density increase loudness, compensate the output stage so the dedicated
  `Level` knob remains useful
- if you add a family-relative alternate mode, keep the same control layout and make the voicing
  shift obvious but not so large that the patch becomes a different effect

### Clean/Dirty Summing Overdrives

Some overdrives, especially the Klon family, should not be modeled as a single clipped path with
tone afterward. Their identity depends on how cleaner and dirtier branches are rebalanced as the
gain control moves.

Design guidance:

- if the original pedal keeps a meaningful clean contribution, make the gain control change both
  clip intensity and branch balance
- low-gain settings should still sound open and stackable, not like a weak version of the max-gain tone
- keep the final sum below runaway loudness before the output stage; the dedicated output control
  should still do real work
- if the pedal uses an active treble shelf after summing, model it as a shelf-style split, not a
  generic one-pole low-pass

### One-Knob Patches and Intentional Unused Controls

Some classic pedals are compelling precisely because they only expose one real control. In those
cases, filling the Endless surface just because extra knobs exist can weaken the patch.

Design guidance:

- if the original effect is genuinely one-knob, prefer preserving that identity over inventing two
  bonus parameters
- document unused controls explicitly so future work does not mistake them for omissions
- if expression must exist because of repo-global routing, mirror the main control or make the lane
  inert; do not silently create a second hidden feature
- mode toggles are a better place to expose historically grounded variants than extra knob functions

### All-Pass Phaser Notes

Classic phasers such as the Phase 90 are best approached as cascaded all-pass stages plus dry
recombination, not as generic modulation effects.

Design guidance:

- use a bounded LFO rate range; too-fast sweeps stop sounding classic quickly
- keep left/right channels linked unless you are intentionally building a stereo reinterpretation
- if a vintage variant removes feedback, let that be the main tonal distinction instead of changing
  multiple unrelated parameters at once

---

## Step 4: Gain Staging

The Endless ADC produces signals normalized to approximately ±1.0 full scale for a line-level
input, with guitar pickups typically ±0.05–0.2 in practice.

**Key principle:** treat the digital ceiling as a hard product constraint, not as the
pedal's main output stage. A dedicated `Level` / `Output` control should still have
headroom to do real work before the final safety limiter matters.

**Checklist:**
- Is the gain applied before clipping realistic? (e.g., 200× on a 0.1 signal = 20 → tanhf
  clips hard, which is intentional for a distortion)
- Does `tanhf` or hard-clip guarantee the output stays in (-1, +1)? Yes.
- Is the final level pot applied after all nonlinearities? It should be.
- Does the clean (bypass) path use the original unmodified input? Yes — store it separately.
- Does the patch still have output headroom after the nonlinear section, or is the
  "Level" control just pushing an already-maxed digital signal into the ceiling?

### Why a hotter output law can still feel worse

Real pedals document their output controls as actual volume stages:

- Ibanez TS-family product pages describe `Drive`, `Tone`, and `Level` controls.
- The MXR Distortion+ manual states that the `OUTPUT` knob controls the overall volume
  of the effect and explicitly tells the player to start with the controls at 12 o'clock.
- The EHX Nano Big Muff Pi manual states that `VOLUME CONTROL — sets the output level`.

That analog expectation does not transfer automatically to Endless if the patch is already
using most of the DAC range inside the nonlinear section. In that situation a hotter digital
output law can look wide in RMS terms while still feeling flat on hardware, because the top
half of the knob is only increasing limiter pressure instead of post-pedal loudness.

Third-pass rule in this fork:

- keep the nonlinear core comfortably below the digital ceiling at default settings
- place the dedicated output control after the nonlinear voicing stage
- aim for bypass-like unity near the middle of the control where that parameter is a true
  output control
- let the final limiter catch only the top edge of travel

### Safety-only output limiter template

Use a limiter that is identity through the normal operating range and only compresses the
top edge:

```cpp
float softLimit(float value)
{
    const float absValue = fabsf(value);
    if (absValue <= 0.90f) {
        return value;
    }

    const float sign = value < 0.0f ? -1.0f : 1.0f;
    const float over = (absValue - 0.90f) / 0.25f;
    return sign * (0.90f + 0.10f * tanhf(over));
}
```

### Output/Level knob template (drive family)

From the third pass tuning in `docs/effects-deep-dive-audit.md`, the dedicated output
controls in this repo now standardize on this shape:

```cpp
// knob in [0, 1] → level curve with a soft knee near 0 so quiet positions are usable
const float levelCurve = level * (0.5f + 0.5f * level);

// base + span·levelCurve gives the primary output sweep;
// (voice.trim − dropoff·driveCurve) is mild drive-dependent compensation.
const float outputGain =
  (base + span * levelCurve) * (voice.trim - dropoff * driveCurve);

// The limiter is safety-only, not the audible output stage:
left[i] = softLimit(processed * outputGain);
```

Current tuned ranges for output controls that target roughly noon = unity:

| Constant   | Range           | Role |
|------------|-----------------|------|
| `base`     | 0.06 – 0.10     | Minimum knob gives a quiet but present signal, not silence |
| `span`     | 0.64 – 0.88     | Gives usable boost above noon without making unity arrive too early |
| `voice.trim` | 0.74 – 0.82   | Per-voice trim that leaves room for the output control to work |
| `dropoff`  | 0.04 – 0.05     | Mild compensation as drive rises; stronger dropoff recenters unity too low |

### Measure output controls by unity and limiter pressure, not only span

For this repo, a drive/output retune is not complete until synthetic analysis and hardware
listening both say the same thing:

- unity lands near the middle of the control
- the upper half of the control increases actual loudness, not only clipping pressure
- the limiter-hit ratio stays low until the top edge of travel
- nonlinear content remains stable during a pure output sweep

The analyzer now supports this with:

- burst RMS and midband RMS
- unity-position detection for the designated output control
- `peak_abs`, `hot_sample_ratio`, and `clip_sample_ratio`

### Equal-power dry/wet crossfade (modulation & filter family)

Linear blends (`dry·(1-mix) + wet·mix`) produce a −3 dB dip at `mix = 0.5`. For any
patch where "more wet" should read as perceptually "more effect" (wah, chorus,
big_muff, back_talk_reverse_delay all use this now), use an equal-power crossfade:

```cpp
constexpr float kHalfPi = 1.5707963f;
const float mix     = clamp01(mix_);
const float dryGain = cosf(mix * kHalfPi);  // 1.0 at mix=0, 0.0 at mix=1
const float wetGain = sinf(mix * kHalfPi);  // 0.0 at mix=0, 1.0 at mix=1
out = dry * dryGain + wet * wetGain;
```

The `cosf`/`sinf` pair is evaluated once per buffer, so the cost is negligible.

### External references used for this doctrine

- [Ibanez TS9 product page](https://www.ibanez.com/jp/products/detail/ts9_99.html):
  control semantics for `Drive`, `Tone`, `Level`
- [MXR Distortion+ manual](https://www.jimdunlop.com/content/manuals/M104.pdf):
  `OUTPUT` is the effect's overall volume and the setup baseline is noon
- [EHX Nano Big Muff Pi manual](https://www.ehx.com/wp-content/uploads/2021/01/nano-big-muff-pi-manual.pdf):
  `VOLUME CONTROL — sets the output level`
- [DAFx paper archive entry for Yeh / Abel / Smith](https://dafx.de/paper-archive/search?author%5B%5D=Smith%2C+J.+O.&author%5B%5D=Yeh%2C+D.+T.&p=1):
  practical filter → nonlinearity → EQ distortion modeling
- [Aalto research entry for Paiva et al. 2012](https://research.aalto.fi/fi/publications/emulation-of-operational-amplifiers-and-diodes-in-audio-distortio/):
  op-amp and diode behavior matter in digital distortion emulation
- [TI product page listing the output-swing white paper and app note](https://www.ti.com/product/OPA392):
  real output stages are constrained by rail headroom, which is the analog-side precedent
  for treating digital headroom as a first-class design constraint here

---

## Step 5: SDK Constraints

| Constraint | Detail |
|---|---|
| No heap allocation | No `new`, `malloc`, `std::vector`, `std::string`. Use fixed arrays or working buffer. |
| Single-precision only | All literals must have `f` suffix. `-fsingle-precision-constant` makes bare `3.14` = float, but `-Wdouble-promotion` will warn. |
| No exceptions / RTTI | `-fno-exceptions -fno-rtti`. Avoid `try/catch`, `dynamic_cast`, `typeid`. |
| Working buffer | 2,400,000 floats (~9.6 MB). Partition in `setWorkingBuffer()`. Pointer is valid until next call. |
| Sample rate | `Patch::kSampleRate = 48000`. Do not hardcode 48000; use the constant. |
| Include path | When in `effects/`: `#include "../source/Patch.h"`. When in `source/PatchImpl.cpp`: `#include "Patch.h"`. |
| No MIDI | The Endless has no MIDI. All modulation comes from knobs, footswitches, or expression pedal. |
| CPU budget | ARM Cortex-M7 @ 720 MHz. Per sample budget at 48 kHz is ~15,000 cycles. `tanhf` costs ~40–80 cycles. A stereo 5-stage chain is fine. |

### Aliasing in Waveshapers

`tanhf()` and hard-clip functions generate harmonics that can fold back below Nyquist
(aliasing). At 48 kHz with maximum distortion gain this is audible as a slightly artificial
sheen on the top end.

The standard fix is **2× oversampling**: upsample to 96 kHz, apply the waveshaper, then
downsample with a half-band FIR filter. This doubles the working buffer usage for the
oversampled stage and requires FIR coefficients. Not currently implemented in this repo.

For embedded use at 48 kHz, `tanhf` aliasing is acceptable in practice — the psychoacoustic
impact at guitar frequencies is mild, and the LP tone filter rolls off some of the aliased
content. Professional studio VST plugins (e.g. Plusdistortion) use 2× oversampling because
they target critical listening at full bandwidth. The Endless hardware context is live
performance, where the trade-off favors simplicity.

### Computing constants outside the sample loop

Filter coefficients (alpha values), gain multipliers, and other parameters derived from knob
positions change at human speed. Recompute them once per audio buffer:

```cpp
void processAudio(std::span<float> left, std::span<float> right) override
{
    // Recompute from current knob values — once per buffer
    float alpha_lp = computeAlphaLP(tone_);
    float gain     = computeGain(dist_);

    for (size_t i = 0; i < left.size(); ++i) {
        // Use precomputed alpha_lp and gain here
    }
}
```

---

## Step 6: Validate with tests/check_patches.sh

Run from the repository root:

```bash
bash tests/check_patches.sh
```

This performs a syntax-only compile check using the host `g++` with the same flags the SDK
uses (except ARM-specific flags). All patches in `effects/` should pass.

---

## Worked Examples in This Repository

### MXR Distortion+ (`effects/mxr_distortion_plus.cpp`)

**Circuit blocks:** HP filter (gain-coupled) → LM741 gain → 1N270 germanium clip → LP tone → Level  
**DSP chain:** 1-pole IIR HP → scalar gain → `tanhf` → 1-pole IIR LP → scalar multiply  
**Key coupling:** Same pot controls both gain and HP cutoff (as in hardware)  
**Reference:** [`docs/mxr-distortion-plus-circuit-analysis.md`](mxr-distortion-plus-circuit-analysis.md)

### Stereo Chorus (`effects/chorus.cpp`)

**Circuit blocks:** Modulated delay line (LFO-swept BBD equivalent)  
**DSP chain:** Write-head → LFO → fractional read (linear interpolation) → dry/wet mix  
**Key detail:** L/R LFOs 90° out of phase for stereo width  

---

## Circuit Variants and DSP Implications

Small circuit differences often require only a targeted change in one DSP stage. The DOD 250
vs. MXR Distortion+ is a clear example: identical topology, different diode.

| | MXR Distortion+ | DOD 250 |
|---|---|---|
| Op-amp | LM741 non-inverting | LM741 non-inverting |
| Gain formula | `1 + 1MΩ / (4.7kΩ + RV)` | `1 + 1MΩ / (4.7kΩ + RV)` |
| Clipping diodes | 1N270 germanium, anti-parallel | 1N914 silicon (some asymmetric) |
| Forward voltage | ~0.3 V | ~0.65 V |
| Knee | Soft, gradual | Sharper |
| DSP | `tanhf(x)` | `tanhf(x * 3.0f)` |
| Character | Warm, smooth overdrive | Tighter, better as clean boost |

**Key lesson:** The diode type (and its forward voltage) determines the `k` scaling factor
in `tanhf(x * k)`, not the circuit topology. When analyzing a new circuit, identify the
diode type first — it's the single biggest predictor of DSP model character.

Other topology-preserving variants to be aware of:
- **Big Muff Pi:** Two cascaded gain+clip stages (not one) → cascade two `tanhf` blocks with
  separate gain stages; produces fuzzier, more sustained distortion
- **Tube Screamer:** Clipping in the op-amp feedback path (not to ground) + strong mid-boost
  → asymmetric clipping model + additional bandpass filter stage
- **Fuzz Face:** Transistor-based (NPN or PNP germanium) → requires asymmetric waveshaper
  and a bias offset term to model the transistor operating point

---

## Advanced Techniques (Not Yet Implemented)

### Wave Digital Filters (WDF)

WDF models each circuit component (resistor, capacitor, diode) as a scattering junction,
enabling component-accurate simulation. The `sthompsonjr` fork of this SDK uses the
`WDF` library. This provides higher fidelity for diode models, inductor-capacitor networks,
and transformer coupling, but is not available in the stock SDK.

WDF reference: K. Werner et al., "Wave Digital Filter Modeling of Circuits with Operational
Amplifiers," EUSIPCO 2016.

**Stability is not free.** WDF circuits can diverge in single precision at 48 kHz for
high-Q sections. The `sthompsonjr` fork shipped a 2026-04-30 fix replacing two WDF
Sallen-Key filters with bilinear-transform biquads after the originals showed a
per-step eigenvalue of ~1.4–2.5× and float overflow within ~261 samples. Diode-pair
clipping has held up better in practice — that is where the local
[`effects/big_muff_wdf.cpp`](../effects/big_muff_wdf.cpp) and
[`effects/tube_screamer_wdf.cpp`](../effects/tube_screamer_wdf.cpp) sibling experiments
live. See
[`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md) for the
full incident notes and the design-pattern discussion.

### Oversampling

Waveshapers (`tanhf`, hard clip) introduce aliasing at high input levels. Inserting a 2×
oversampled processing stage (upsample → clip → downsample) reduces aliasing artifacts.
Not currently implemented; would require FIR filter coefficients and 2× working buffer usage.

### Biquad Filters (2nd-order IIR)

For parametric EQ, resonant filters, or accurate wah emulation, use the standard transposed
Direct Form II Biquad:

```cpp
// Coefficients: b0, b1, b2, a1, a2 (normalized, a0=1)
float w  = x  - a1*s1 - a2*s2;
float y  = b0*w + b1*s1 + b2*s2;
s2 = s1; s1 = w;
```

Coefficient calculation tools: Audio EQ Cookbook (Robert Bristow-Johnson).

---

## Quick Reference Card

```
HP filter:  alpha = 1 / (1 + 2π*fc/fs)
            y[n]  = alpha * (y[n-1] + x[n] - x[n-1])

LP filter:  omega = 2π*fc/fs
            alpha = omega / (1 + omega)
            y[n]  = alpha*x[n] + (1-alpha)*y[n-1]

Soft clip:  y = tanhf(x)                  // germanium (k=1), tube
            y = tanhf(x * 3.0f)           // silicon (k=3), harder knee
Hard clip:  y = fmaxf(-1, fminf(1, x))    // LED, zener
Asymmetric: y = (x>=0) ? tanhf(x) : -tanhf(-x*3)  // mixed diodes
Gain:       y = x * (1 + R2/Ri)           // non-inverting amp

Log taper:  hz = minHz * powf(maxHz/minHz, knob)
            (computed once per buffer with powf)
```

---

## References

- **Yeh & Abel (DAFX 2007)** — "Simplified, Physically-Informed Models of Distortion and
  Overdrive Guitar Effects Pedals." Canonical academic reference for the MXR D+, DOD 250,
  and similar circuits: https://ccrma.stanford.edu/~dtyeh/papers/yeh07_dafx_distortion.pdf
- **Werner et al. (EUSIPCO 2016)** — "Wave Digital Filter Modeling of Circuits with
  Operational Amplifiers." Foundational WDF paper for op-amp circuits.
- **ChowDSP WDF library** — Open-source C++ WDF implementation:
  https://github.com/Chowdhury-DSP/chowdsp_wdf
- **Audio EQ Cookbook** (R. Bristow-Johnson) — Biquad coefficient formulas for all standard
  filter types: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
- **FAUST** — Functional DSP language for rapid algorithm prototyping, compiles to C++:
  https://faust.grame.fr/
- **ElectroSmash** — Detailed circuit analyses of classic pedals (MXR D+, Big Muff, Tube
  Screamer, etc.): https://www.electrosmash.com/
