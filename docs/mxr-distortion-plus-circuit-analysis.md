# MXR Distortion+ Circuit Analysis & DSP Model

## Overview

The MXR Distortion+ (MXR M104) is one of the most studied and widely cloned guitar distortion
pedals ever made. Its circuit is straightforward — a single op-amp gain stage followed by diode
clipping — which makes it an excellent reference case for learning circuit-to-DSP conversion.

This document covers the original analog circuit, the component-level analysis that informs the
DSP model, and the implementation decisions made for `effects/mxr_distortion_plus.cpp`.

---

## Circuit Topology

```
Input → [HP filter] → [LM741 non-inverting amp] → [1N270 diode clip] → [LP tone filter] → [Level pot] → Output
```

The circuit has five functional blocks:

| Block | Analog implementation | DSP equivalent |
|---|---|---|
| Input coupling / HP | C1 (47 nF) + R_input (1 MΩ pot + 4.7 kΩ) | 1-pole IIR high-pass |
| Gain | LM741 non-inverting: `1 + R2/Ri` | Scalar multiply |
| Clipping | 1N270 germanium diodes to ground | `tanhf()` soft clip |
| Tone / LP | C2 + resistor network | 1-pole IIR low-pass |
| Level | 100 kΩ pot | Scalar multiply |

---

## Block-by-Block Analysis

### 1. Input High-Pass Filter

**Analog:** Capacitor C1 = 47 nF in series with the input, forming a high-pass filter. The
resistive component is the sum of a fixed resistor (4.7 kΩ) and the Distortion potentiometer
(0–1 MΩ), which couples the HP cutoff to the gain setting.

**Cutoff frequency:**

```
fc_hp = 1 / (2π × C1 × R_total)
      = 1 / (2π × 47e-9 × (4700 + RV))
```

Where `RV` is the wiper position of the Distortion pot (0 Ω = max distortion, 1 MΩ = min distortion).

**Cutoff range:**

| Distortion knob | RV | fc_hp |
|---|---|---|
| Maximum (knob full CW) | 0 Ω | ~720 Hz |
| Minimum (knob full CCW) | 1 MΩ | ~3.4 Hz |

At maximum distortion, the high-pass corner rises to ~720 Hz, rolling off low-end muddiness.
This is a key character of the Distortion+ sound at high gain settings.

**DSP (1-pole IIR HP, Euler method):**

```cpp
float alpha_hp = 1.0f / (1.0f + kTwoPi * fc_hp / kSampleRate);
// Per sample:
float hpOut = alpha_hp * (hpPrev + x - xPrev);
xPrev = x;
hpPrev = hpOut;
```

### 2. Op-Amp Gain Stage (LM741)

**Analog:** The LM741 is configured as a non-inverting amplifier. The gain is set by two
resistors in the feedback network:

```
gain = 1 + R2 / Ri
```

Where:
- R2 = 1 MΩ (fixed feedback resistor)
- Ri = 4.7 kΩ (fixed) + RV (Distortion pot, 0–1 MΩ)

**Gain range:**

| Distortion | RV | Ri | Gain | dB |
|---|---|---|---|---|
| Maximum | 0 Ω | 4.7 kΩ | ~213× | ~46 dB |
| Minimum | 1 MΩ | 1.0047 MΩ | ~2× | ~6 dB |

The gain and HP cutoff are controlled by the same physical pot, just as in the hardware.
This coupling is preserved in the DSP model (both computed from the same `dist_` parameter).

**DSP:**

```cpp
float Ri = 4700.0f + (1.0f - dist_) * 1e6f;
float gain = 1.0f + 1e6f / Ri;
float gained = hpOut * gain;
```

### 3. Diode Clipping (1N270 Germanium)

**Analog:** Two 1N270 germanium diodes connected in anti-parallel (one forward, one reverse)
from the output of the op-amp to ground. When the signal exceeds the diode forward voltage
(~0.3 V for germanium vs ~0.6 V for silicon), the diodes conduct and clamp the signal.

**Germanium vs silicon clipping behavior:**

- **Germanium (1N270):** Lower forward voltage, softer knee → smooth, rounded clipping with
  gradual onset. The diode current rises as a smooth exponential, producing soft saturation.
- **Silicon (1N914, 1N4148):** Higher forward voltage, sharper knee → harder clipping,
  closer to hard limiting.

**DSP model:** `tanhf()` is the standard approximation for soft diode/tube saturation. Its
S-curve shape matches the gradual onset and smooth saturation of germanium diodes reasonably
well. The output is bounded to (-1, +1).

```cpp
float clipped = tanhf(gained);
```

**Why not hard clip (`fminf/fmaxf`):** Hard clipping (squaring off the waveform) produces
strong odd harmonics with an abrupt transition. Germanium diodes produce a softer spectrum
more consistent with `tanh`.

**Gain staging note:** Guitar signals at line level after ADC normalization are typically in
the range ±0.05–0.1. At maximum gain (213×), a 0.1 input reaches 21.3 before `tanhf`, which
saturates fully to ~1.0. At minimum gain (2×), a 0.1 input reaches 0.2, and `tanhf(0.2) ≈
0.198`, which is nearly linear. This means the full gain range from clean to hard clip is
covered with no input prescaling required.

### 4. Output Low-Pass / Tone Filter

**Analog:** A passive RC low-pass filter after the clipping stage. The Tone potentiometer
sets the cutoff frequency. At the stock position, this is approximately 15.9 kHz.

**DSP (1-pole IIR LP):**

```cpp
float fc_lp = 3000.0f + tone_ * 17000.0f;  // 3 kHz to 20 kHz
float omega = kTwoPi * fc_lp / kSampleRate;
float alpha_lp = omega / (1.0f + omega);
// Per sample:
float lpOut = alpha_lp * clipped + (1.0f - alpha_lp) * lpPrev;
lpPrev = lpOut;
```

The tone range (3–20 kHz) allows rolling off the harsh upper harmonics produced by heavy
clipping, approximating the original tone circuit.

### 5. Level

**Analog:** 100 kΩ potentiometer at the output, acting as a simple voltage divider.

**DSP:**

```cpp
float out = lpOut * level_;
```

The expression pedal is mapped to this parameter (heel = 0, toe = full level).

---

## Filter Coefficient Computation

Both filter alpha values depend on the Distortion and Tone control settings. In the SDK
implementation, these are computed **once per audio buffer** (outside the sample loop), not
per sample. This is an important optimization: computing `tanhf` per sample is unavoidable,
but filter coefficient updates at 48 Hz (once per 1000-sample buffer) rather than 48 kHz
costs nothing audible.

```cpp
void processAudio(std::span<float> left, std::span<float> right) override
{
    // Compute coefficients once per buffer
    float Ri     = 4700.0f + (1.0f - dist_) * 1e6f;
    float gain   = 1.0f + 1e6f / Ri;
    float fc_hp  = 1.0f / (kTwoPi * 47e-9f * Ri);
    float alpha_hp = 1.0f / (1.0f + kTwoPi * fc_hp / kSampleRate);

    float fc_lp  = 3000.0f + tone_ * 17000.0f;
    float omega_lp = kTwoPi * fc_lp / kSampleRate;
    float alpha_lp = omega_lp / (1.0f + omega_lp);

    // Then process each sample using these constants
    for (size_t i = 0; i < left.size(); ++i) { ... }
}
```

---

## Limitations of the DSP Model

| Aspect | Analog behavior | DSP approximation | Fidelity |
|---|---|---|---|
| Diode clipping shape | Exponential I-V curve | `tanhf()` | Good — smooth soft-clip |
| Diode asymmetry | Matched pair, near-symmetric | Fully symmetric | Good |
| Op-amp saturation | LM741 rail limiting | Not modelled | Minor — diodes clip first |
| Frequency-dependent clipping | Diode capacitance modifies HF response | Not modelled | Minor |
| Power supply sag | Voltage droop under heavy load | Not modelled | Minor |
| Op-amp noise / non-linearity | LM741 characteristic noise floor | Not modelled | Minor |
| Tone control interaction | Passive network with loading | Simplified LP only | Acceptable |
| Op-amp nonlinearity (LM741) | Rail-limiting, slew-rate distortion, bandwidth ~1 MHz | Not modelled separately | Minor — diodes clip first |
| Aliasing from `tanhf` | Harmonic content above Nyquist at max gain | No oversampling applied | Audible at maximum Distortion setting |

For higher fidelity, Wave Digital Filter (WDF) techniques can model the component-level
behavior more accurately (see sthompsonjr's fork, which uses the `WDF` library). However,
WDF is not available in the stock SDK.

---

## Related Circuits — DOD 250 Comparison

The DOD 250 Overdrive Preamp is the closest circuit relative to the MXR Distortion+. Both
use an LM741 op-amp in a non-inverting configuration with essentially the same gain formula.
The critical difference is the clipping diodes:

| Feature | MXR Distortion+ | DOD 250 |
|---|---|---|
| Clipping diodes | 1N270 germanium, anti-parallel to ground | 1N914 silicon (some versions asymmetric) |
| Forward voltage | ~0.3 V | ~0.65 V |
| Knee character | Soft, gradual onset | Sharper, more abrupt |
| Tonal character | Warm, smooth overdrive; less output volume | Tighter, more aggressive; stronger as clean boost |
| DSP model | `tanhf(x)` — k=1 | `tanhf(x * 3.0f)` — higher k for sharper knee |

### DSP implications

The diode type determines the scaling factor `k` in the waveshaper:

```cpp
// MXR Distortion+ — germanium, soft clip (k=1)
float clipped = tanhf(gained);

// DOD 250 equivalent — silicon, harder clip (k≈3)
float clipped = tanhf(gained * 3.0f);

// DOD 250 asymmetric variant (single diode each polarity, unmatched)
float clipped = (gained >= 0.0f)
    ? tanhf(gained * 3.0f)
    : -tanhf(-gained * 2.5f);   // slightly softer negative half
```

### Character difference

At low distortion settings the DOD 250 functions well as a unity-gain or mild boost pedal —
the silicon diodes stay below their threshold, producing cleaner amplification than the MXR.
At high gain the MXR D+ produces a warmer, more compressed saturation; the DOD 250 produces
a brighter, slightly rawer tone.

This repository includes one stock-SDK-compatible implementation of this circuit family in
`effects/mxr_distortion_plus.cpp`. A DOD 250 variant could be implemented by changing the
clipping stage and adjusting the `k` factor as shown above.

---

## Parameter Mapping Summary

| Hardware control | Knob | SDK param | Computed value |
|---|---|---|---|
| Distortion pot | Left | `kParamLeft` (0) | Controls RV → drives both gain and HP fc |
| Tone pot | Mid | `kParamMid` (1) | Controls LP cutoff (3–20 kHz) |
| Level pot | Right | `kParamRight` (2) | Output gain 0–1; also expression pedal |

Expression pedal: heel down = 0.0 (silent), toe down = 1.0 (full level). This mapping is
handled by `internal/PatchCppWrapper.cpp` which routes the expression pedal to param 2
(Right knob) for all patches in this repository.

---

## References

- ElectroSmash — MXR Distortion+ Analysis: https://www.electrosmash.com/mxr-distortion-plus-analysis
- 1N270 Germanium Diode datasheet (forward voltage ~0.3 V, soft knee characteristic)
- LM741 Op-Amp datasheet (non-inverting configuration, open-loop gain ~200 V/mV)
- Yeh & Abel, "Simplified, Physically-Informed Models of Distortion and Overdrive Guitar Effects Pedals," DAFX 2007 (CCRMA Stanford) — canonical academic reference for this class of circuits: https://ccrma.stanford.edu/~dtyeh/papers/yeh07_dafx_distortion.pdf
- Plusdistortion VST by Distorque Audio — C++ MXR Distortion+ emulation with 2× oversampling and separate op-amp / diode clipping controls: http://distorqueaudio.com/plugins/plusdistortion.html
- See also: [`docs/circuit-to-patch-conversion.md`](circuit-to-patch-conversion.md) for the general methodology

---

## Related Files

- `effects/mxr_distortion_plus.cpp` — SDK patch implementation
- `docs/circuit-to-patch-conversion.md` — General analog-to-DSP methodology
- `internal/PatchCppWrapper.cpp` — Expression pedal routing (param 2)
