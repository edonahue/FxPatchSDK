# Circuit-to-Patch Conversion: Methodology Guide

This document captures the methodology for converting analog guitar effect circuits into
Polyend Endless SDK patches. It is written for future LLM sessions and human developers
who want to replicate or design new effects based on real circuit analysis.

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

### Op-Amp Gain Stage → Scalar Multiply

**Non-inverting configuration:** `gain = 1 + R_feedback / R_input`  
**Inverting configuration:** `gain = -R_feedback / R_input` (invert sign)

```cpp
float gained = x * gain;
```

When gain is controlled by a potentiometer, recompute `gain` once per buffer, not per sample.

### Diode Clipping → Waveshaper

Diode clipping is nonlinear — the DSP equivalent is a waveshaper function applied to each sample.

| Clipping type | Analog component | DSP approximation |
|---|---|---|
| Soft symmetric | Germanium diodes (1N270, 1N34A) | `tanhf(x)` |
| Soft symmetric | Vacuum tube triode | `tanhf(x)` or polynomial |
| Medium symmetric | Silicon diodes (1N914, 1N4148) | `tanhf(x * k)` with higher k, or piecewise |
| Hard symmetric | LED clipping | `fmaxf(-1, fminf(1, x))` |
| Asymmetric | Mixed diode types or tube | Asymmetric tanh: positive/negative separate |

**Standard soft clip:**
```cpp
float clipped = tanhf(gained);  // always in (-1, +1)
```

**Harder soft clip (higher-order approximation):**
```cpp
// Cubic soft clip — less expensive than tanhf, clips harder
float clipped = x - (x * x * x) / 3.0f;  // valid only for |x| <= 1
```

**Hard clip:**
```cpp
float clipped = fmaxf(-1.0f, fminf(1.0f, gained));
```

**Asymmetric (positive path softer than negative, or vice versa):**
```cpp
float clipped = (gained >= 0.0f)
    ? tanhf(gained)                        // soft positive clip
    : fmaxf(-1.0f, gained * 1.5f);         // harder negative clip
```

### Delay-Based Effects → Working Buffer

For reverb, chorus, flanger, echo: use the working buffer provided by the SDK.
See `source/Patch.h` for `kWorkingBufferSize = 2400000` floats (~9.6 MB).

```cpp
void setWorkingBuffer(float* buf, int size) override {
    delayL_ = buf;          // partition the buffer
    delayR_ = buf + kLen;
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
| Sample rate | `endless::kSampleRate = 48000`. Do not hardcode 48000; use the constant. |
| Include path | When in `effects/`: `#include "../source/Patch.h"`. When in `source/PatchImpl.cpp`: `#include "Patch.h"`. |
| No MIDI | The Endless has no MIDI. All modulation comes from knobs, footswitches, or expression pedal. |
| CPU budget | ARM Cortex-M7 @ 720 MHz. Per sample budget at 48 kHz is ~15,000 cycles. `tanhf` costs ~40–80 cycles. A stereo 5-stage chain is fine. |

### Computing constants outside the sample loop

Filter coefficients (alpha values), gain multipliers, and other parameters derived from knob
positions change at human speed. Recompute them once per audio buffer:

```cpp
void processAudio(const float* inL, const float* inR,
                  float* outL, float* outR, int numSamples) override
{
    // Recompute from current knob values — once per buffer
    float alpha_lp = computeAlphaLP(tone_);
    float gain     = computeGain(dist_);

    for (int i = 0; i < numSamples; ++i) {
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

Soft clip:  y = tanhf(x)                  // germanium, tube
Hard clip:  y = fmaxf(-1, fminf(1, x))    // silicon LED
Gain:       y = x * (1 + R2/Ri)           // non-inverting amp

Log taper:  hz = minHz * powf(maxHz/minHz, knob)
            (computed once per buffer with powf)
```
