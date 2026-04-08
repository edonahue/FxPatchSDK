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

---

## Step 3: Determine Control Mappings

The Endless has three knobs (Left, Mid, Right) and one expression pedal.
`internal/PatchCppWrapper.cpp` in this repository routes the expression pedal to param 2
(Right knob) for all patches.

**Mapping strategy:**

1. Identify the 2–3 most musically important controls in the original circuit.
2. Assign them to knobs in decreasing order of importance (Left = most important).
3. If a parameter has a wide logarithmic range (e.g., frequency or gain), apply a log taper
   to make the knob feel even across its travel.
4. Reserve Right knob (param 2) for level or mix if you want expression pedal control.

**Log taper example:**
```cpp
// Rate knob: 0.1 Hz at 0.0, 5 Hz at 1.0
float hz = 0.1f * powf(50.0f, rate_);
```

**Frequency range mapping:**
```cpp
// Tone knob: 3 kHz at 0.0, 20 kHz at 1.0
float fc = 3000.0f + tone_ * 17000.0f;
```

---

## Step 4: Gain Staging

The Endless ADC produces signals normalized to approximately ±1.0 full scale for a line-level
input, with guitar pickups typically ±0.05–0.2 in practice.

**Key principle:** keep your signal in the range (-2, +2) before any waveshaper, and ensure
the final output does not exceed ±1.0.

**Checklist:**
- Is the gain applied before clipping realistic? (e.g., 200× on a 0.1 signal = 20 → tanhf
  clips hard, which is intentional for a distortion)
- Does `tanhf` or hard-clip guarantee the output stays in (-1, +1)? Yes.
- Is the final level pot applied after all nonlinearities? It should be.
- Does the clean (bypass) path use the original unmodified input? Yes — store it separately.

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
