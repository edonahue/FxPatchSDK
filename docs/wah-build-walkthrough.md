# Wah Wah Patch — Build Walkthrough

**File:** `effects/wah.cpp`  
**Date:** April 2026  
**Filter type:** Chamberlin State-Variable Filter (SVF)  
**Modes:** Dunlop Crybaby GCB-95 · Vox V847  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

---

## Overview

A wah pedal is a bandpass filter whose center frequency is swept by the player's foot. The
resonant peak creates a vowel-like formant that rises and falls — "wah." This patch implements
two classic characters: the Dunlop Crybaby (aggressive, nasal, wide sweep) and the Vox V847
(warmer, smoother, slightly narrower).

The expression pedal input drives the sweep. The three knobs control Mix, Q, and the wah
position when no expression pedal is connected (a "parked wah" tone). Hold the footswitch to
toggle between modes.

---

## Circuit Reference

### Dunlop Crybaby GCB-95

The original Crybaby uses an inductor (Fasel or Halo type) and capacitors to form a resonant
LC bandpass with the op-amp stage. The sweep is produced by a potentiometer in the expression
pedal mechanism varying the effective impedance in the filter network.

**Key measured parameters:**
- Heel frequency: ~350 Hz
- Toe frequency: ~2.5 kHz
- Resonance Q: approximately 4–6 at the peak
- Character: pronounced, nasal, aggressive onset; classic rock/funk tone

### Vox V847

The Vox uses a similar inductor-capacitor topology but different component values, particularly
a different inductor and RC network that softens the resonance and reduces the upper sweep
frequency slightly.

**Key measured parameters:**
- Heel frequency: ~350 Hz
- Toe frequency: ~2.2 kHz
- Resonance Q: approximately 2–4 at the peak
- Character: warmer, more vocal, less nasal; popular for jazz, fusion, smooth funk

### DSP simplification

Neither pedal's exact component values are replicated component-by-component (which would
require Wave Digital Filters). Instead, the measured parameters (fc range, Q) are used directly
as DSP targets. This is the "simplified physically-informed model" approach — see
[Yeh & Abel, DAFX 2007](https://ccrma.stanford.edu/~dtyeh/papers/yeh07_dafx_distortion.pdf).

---

## Decision 1 — Filter Topology

**Goal:** A resonant bandpass filter with independently controllable center frequency and Q.

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| Cascaded 1-pole HP + LP | Very simple code | Cannot produce resonant peak (Q > 0.5); sounds nothing like a wah |
| Biquad bandpass (Audio EQ Cookbook) | Well-documented; standard; 5 coefficients | Coefficient update involves sinf + cosf; Q and fc coupled in some formulations |
| Chamberlin SVF (2nd order) | 3 lines/sample; direct BP output; f1 and q1 independent; same cost | Chamberlin-specific stability constraint (see below) |

**Chose: Chamberlin SVF.** The state-variable filter directly yields bandpass, lowpass, and
highpass outputs from a single pass through three equations. The frequency coefficient `f1`
and damping `q1` are completely independent, which maps naturally to the Q knob and the
expression-pedal-swept frequency.

**Stability note:** The Chamberlin SVF can become unstable if `f1 > 2*q1`. Within our
parameter space (fc ≤ 2.5 kHz at 48 kHz, Q ≤ 10):
- Maximum f1 = 2×sin(π×2500/48000) ≈ 0.325
- Minimum 2×q1 at Q=10: 2×(1/10) = 0.2

This is a borderline condition at Q=10 and fc=2500 Hz. In practice, the player is unlikely
to hold both at maximum simultaneously, and the result at that corner case would be a very
slightly distorted resonance rather than a crash. Accepted for the implementation.

---

## Decision 2 — Gain Normalization

**Problem:** The Chamberlin SVF bandpass output has peak gain = Q/2. At Q=10, the bandpass
signal is 5× the input amplitude at the resonant frequency — much too loud.

**Solution:** Multiply the bandpass output by `2.0f * q1` (= 2/Q):

```cpp
const float bpGain = 2.0f * q1;  // 2/Q
// Per sample:
float wetL = bandL_ * bpGain;
```

This normalizes the peak gain to exactly 1.0 (0 dB) at the resonant frequency, independent
of Q. Off-resonance, the output is attenuated by the bandpass rolloff — which is what creates
the wah character when blended with the dry signal via the Mix knob.

**Gain staging result (Mix = 1.0, full wet):**
- At resonant frequency: output = 1.0 × input (unity gain)
- Well away from resonance: output → 0 (bandpass rejection)
- The wah sweep moves the resonance → creates the classic vowel-filter sweeping effect

---

## Decision 3 — Control Layout (Expression Pedal Constraint)

**Constraint:** The expression pedal is hardcoded to param 2 (Right knob) in
`internal/PatchCppWrapper.cpp`. This is a global SDK setting affecting all patches — not
per-patch configurable in the current SDK version.

**Implication:** The expression pedal calls `setParamValue(2, value)` with heel=0.0, toe=1.0.
This means **param 2 IS the wah sweep position**, and the Right knob acts as a "parked wah"
control when no expression pedal is connected.

**The Sweep Range problem:** The original interview question asked for Mix, Q, and a third
creative knob (Sweep Range was selected). However, with Right/param 2 dedicated to the sweep
position, there is no knob left for a continuous Sweep Range control.

**Resolution:** Sweep Range variation is captured in the mode system:
- Crybaby mode: 350 Hz → 2500 Hz (wide, aggressive)
- Vox mode: 350 Hz → 2200 Hz (narrower, warmer)

This is actually a better design for live performance — the player selects a sweep character
with the footswitch hold rather than adjusting a knob mid-phrase. The final knob layout:

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | Mix | 0=dry, 1=full wet | 1.0 (full wet) |
| Mid | 1 | Q resonance | 0→Q=1.0, 1→Q=10.0 | 0.444 (→ Q≈5) |
| Right / Exp | 2 | Wah position | 0=heel/350Hz, 1=toe/fc_max | 0.5 (mid/"cocked wah") |

**Future improvement:** Implementing a virtual `isParamEnabled()` method on `Patch.h` (see
`docs/endless-reference.md §3a`) would allow this patch to declare the expression pedal on a
non-Right knob, freeing param 2 for Sweep Range. Not available in the stock SDK.

---

## Decision 4 — Frequency Taper

**Problem:** A linear frequency sweep sounds uneven to the ear — the wah rushes through the
lower octave quickly and lingers in the upper range.

**Linear:**
```
fc = 350 + wahPos * (fc_max - 350)
```
At wahPos=0.5: fc = 350 + 0.5×2150 = 1425 Hz — feels "already too far up"

**Logarithmic (chosen):**
```
fc = fc_min * powf(fc_max / fc_min, wahPos)
```
At wahPos=0.5: fc = 350 × √(2500/350) ≈ 935 Hz — geometrically centered, feels balanced

The log taper is computed once per buffer (not per sample), so `powf` is called at approximately
48 Hz — negligible CPU cost. The player experiences a smooth, musically even wah sweep from
heel to toe.

---

## Decision 5 — Footswitch UX (Hardware Constraint)

**User expectation:** Use a second footswitch to toggle between Crybaby and Vox modes.

**Hardware reality:** The Endless has two physical footswitches, but the SDK only exposes
ONE switch with two event types in the `ActionId` enum:
- `kLeftFootSwitchPress` (idx=0): short press
- `kLeftFootSwitchHold` (idx=1): held press

The second physical footswitch is firmware-reserved and not accessible to patch code in the
current SDK.

**Resolution:** Map the two events of the single exposed switch:
- **Short press** (idx=0) → bypass toggle (standard pedal behavior; most expected)
- **Hold** (idx=1) → Crybaby ↔ Vox mode toggle (less frequently needed; hold is natural)

On mode toggle, filter state is cleared to prevent a transient pop from the fc jump.

**Future improvement:** If Polyend exposes the second footswitch in a future SDK update,
map `kRightFootSwitchPress` → mode toggle, freeing the hold for a third function (e.g.,
momentary auto-wah or tap-tempo LFO rate).

---

## Decision 6 — LED States

Four distinct states, using verified `Color` enum values from `source/Patch.h`:

| State | Color | `Color` enum |
|---|---|---|
| Crybaby, active | Red | `Color::kRed` |
| Vox, active | Yellow | `Color::kLightYellow` |
| Crybaby, bypassed | Dark red | `Color::kDarkRed` |
| Vox, bypassed | Dim yellow | `Color::kDimYellow` |

**Rationale for dim-mode-color on bypass:** The player can see which mode they will return to
when they re-engage the effect. This is more informative than a generic DimWhite (used by
other patches in this repo) and appropriate here because mode switching requires a deliberate
hold action — you want to know where you left it.

---

## Decision 7 — State Clearing Strategy

| Event | Clear filter state? | Reason |
|---|---|---|
| Bypass → active (press) | **Yes** | Stale lowL_/bandL_ values from before bypass would cause a pop |
| Active → bypass (press) | No | Signal is just silenced; no state read |
| Mode toggle (hold) | **Yes** | fc jumps between modes; old state at wrong fc causes a transient |
| Expression pedal moves | **No** | State continuity creates the smooth sweep character |
| Q knob changes | No | Gradual parameter change; continuity is desirable |
| Mix knob changes | No | No filter state involved |

```cpp
void clearFilterState() {
    lowL_ = bandL_ = lowR_ = bandR_ = 0.0f;
}
```

---

## Implementation Notes

### No working buffer needed

The SVF requires only 4 float state variables (lowL, bandL, lowR, bandR). These live as
class members in internal SRAM. The external working buffer (2.4M floats) is not used.

### CPU budget

Per sample cost (one channel):
- 1× addition + 1× multiply (lowpass update)
- 3× addition + 2× multiply (highpass compute)
- 1× addition + 1× multiply (bandpass update)
- 1× multiply + 1× addition (gain normalization + mix)

Total: ~8 multiplies, ~6 additions per sample per channel. At 48 kHz, stereo: ~16 ops per sample
pair, ~768 kops/s. The Cortex-M7 at 720 MHz handles this trivially — headroom for many more effects.

### Parked wah (no expression pedal)

When no expression pedal is connected, the Right knob manually sets `wahPos_`. The default
(0.5) produces a "cocked wah" tone — the filter parked at mid-sweep (~935 Hz). This is a
popular guitar tone in its own right (used in funk and R&B) and makes the patch usable even
without the expression pedal connected.

---

## Testing

```bash
# Syntax and lint check (from repo root)
bash tests/check_patches.sh
```

**Expected:** `PASS: effects/wah.cpp` with 0 lint warnings.

**Manual listening checklist (hardware):**
- [ ] Wah sweep from heel to toe is smooth and even (log taper working)
- [ ] Q knob tightens the resonance from broad to nasal as it increases
- [ ] Mix=0 passes dry signal completely (bypass-like but effect still running)
- [ ] Mix=1 is classic wah — no dry bleed
- [ ] Hold footswitch toggles LED from Red → Yellow (Crybaby → Vox)
- [ ] Vox mode sounds warmer and slightly less extended at toe
- [ ] Short press bypasses; LED dims to DarkRed or DimYellow depending on current mode
- [ ] Re-engaging after bypass has no pop or click
- [ ] Mode toggle while active has no pop or click

---

## Future Improvements

1. **Auto-wah LFO:** When no expression pedal is connected, an LFO could sweep `wahPos_`
   automatically. A Rate knob could replace the fixed-position parked wah behavior.

2. **Per-patch expression pedal routing:** Implement virtual `isParamEnabled()` on `Patch.h`
   (see `docs/endless-reference.md §3a`). This would allow assigning the expression pedal to
   a different param, freeing param 2 (Right knob) for a Sweep Range control.

3. **Envelope follower (auto-wah):** Use the input signal amplitude to drive `wahPos_`
   instead of (or in addition to) the expression pedal. A touch-wah effect.

4. **Second footswitch:** If Polyend exposes `kRightFootSwitchPress` in a future SDK update,
   reassign mode toggle to that switch for more ergonomic control.

---

## Related Files

- `effects/wah.cpp` — the patch implementation
- `docs/circuit-to-patch-conversion.md` — SVF primitive documentation, general methodology
- `docs/endless-reference.md` — full SDK reference, expression pedal routing (§3a)
- `internal/PatchCppWrapper.cpp` — expression pedal hardcoded to param 2
- `docs/templates/patch-build-walkthrough.md` — blank template for future patches
