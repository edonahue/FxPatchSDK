# Effect Examples

Reference implementations collected from community forks of the Polyend FxPatchSDK.
Useful for understanding patterns, techniques, and what's possible on the Endless hardware.

---

## Files

| File | Effect | Source | Compiles against stock SDK? |
|---|---|---|---|
| [`reverb.cpp`](reverb.cpp) | Freeverb stereo reverb | andybalham/FxPatchSDK | Yes |
| [`optical_compressor.cpp`](optical_compressor.cpp) | LA-2A / Leveler / Hybrid compressor | sthompsonjr/Endless-FxPatchSDK | No* |
| [`tubescreamer.cpp`](tubescreamer.cpp) | TS808 / TS9 / Klon overdrive | sthompsonjr/Endless-FxPatchSDK | No* |
| [`wah.cpp`](wah.cpp) | Cry Baby wah — expression, auto, LFO modes | sthompsonjr/Endless-FxPatchSDK | No* |

\* The sthompsonjr files depend on WDF circuit headers and a modified SDK that aren't
in the stock `polyend/FxPatchSDK`. See the **Dependency Notes** section below.

---

## Effect Summaries

### `reverb.cpp` — Freeverb Stereo Reverb
**Source:** [andybalham/FxPatchSDK](https://github.com/andybalham/FxPatchSDK)

Classic Schroeder/Freeverb algorithm — 4 parallel feedback comb filters, 2 series
all-pass filters, separate left/right delay line lengths for stereo width. All delay
buffers (~50 KB) live in the working buffer (external RAM).

**Knobs:** Room Size · Damping · Dry/Wet Mix  
**Footswitch:** Bypass toggle  
**What to study:** `setWorkingBuffer()` pattern for delay-line allocation, comb + all-pass
filter implementation, bypass with LED feedback.

---

### `optical_compressor.cpp` — Optical Compressor
**Source:** [sthompsonjr/Endless-FxPatchSDK](https://github.com/sthompsonjr/Endless-FxPatchSDK)

Three selectable compression topologies on the right knob: PC-2A (LA-2A-style linked
stereo optical), Optical Leveler (single-knob), and Hybrid Opt/OTA (optical detector
feeding an OTA VCA). Attack and release modes toggled via footswitches.

**Knobs:** Peak Reduction · Makeup Gain · Circuit Select  
**Expression:** HF Emphasis (treble-weighted sidechain)  
**Footswitch:** Attack fast/slow toggle · Release short/long toggle  
**What to study:** Multi-circuit architecture, footswitch toggling time constants,
LED color as a compression gain-reduction meter.

---

### `tubescreamer.cpp` — Tubescreamer Family
**Source:** [sthompsonjr/Endless-FxPatchSDK](https://github.com/sthompsonjr/Endless-FxPatchSDK)

Three vintage overdrive circuits selectable via right knob: Ibanez TS808, TS9, and
Klon Centaur clip stage. Age mode adds worn op-amp character; Asymmetry mode adds
even-harmonic content and shifts the input high-pass for vintage vs. modern feel.
Uses a `ParameterSmoother` on the level control to prevent audible clicks.

**Knobs:** Drive · Tone · Circuit Select  
**Expression:** Level  
**Footswitch:** Age mode toggle · Asymmetry toggle  
**What to study:** Multi-circuit state management with `applyCurrentParams()` on circuit
switch, parameter smoothing, combined LED states (age + asym = different colors).

---

### `wah.cpp` — Cry Baby Wah
**Source:** [sthompsonjr/Endless-FxPatchSDK](https://github.com/sthompsonjr/Endless-FxPatchSDK)

Cry Baby LC resonant filter with four modes: expression pedal sweep, envelope follower
(auto-wah), LFO sweep, and EnvLfo (envelope controls LFO rate). Tap tempo on the
hold footswitch sets LFO rate by timing between holds. Sweep direction is reversible.

**Knobs:** Resonance · Sensitivity · Mode Select  
**Expression:** Sweep position (heel ↔ toe)  
**Footswitch:** Sweep direction reverse · Tap tempo  
**What to study:** Sample counter for tap-tempo timing, 4-way mode dispatch via right
knob, expression pedal routing, LED color overriding on extreme resonance.

---

## Dependency Notes

### reverb.cpp
No external dependencies. Compiles against stock SDK with this include:
```cpp
#include "../../source/Patch.h"
```
Copy to `source/PatchImpl.cpp` (rename class to `PatchImpl`, or rename the
`getInstance` return) and build normally.

### sthompsonjr effects (optical_compressor, tubescreamer, wah)
These require sthompsonjr's full fork which includes:

- **Modified SDK** (`sdk/Patch.h`) — adds `isParamEnabled(int, ParamSource)` to the
  `Patch` base class. This virtual method lets each circuit declare which parameters are
  active and from which source (knob vs. expression pedal). Not present in stock SDK.
- **WDF libraries** (`wdf/` directory) — Wave Digital Filter implementations of
  analog circuits (optical compressors, Tubescreamer op-amp stages, Cry Baby LC filter).
- **DSP utilities** (`dsp/ParameterSmoother.h`, `dsp/Saturation.h`) — parameter
  smoothing and soft-clip helpers.

To use these effects, clone sthompsonjr's repo directly:
```
git clone https://github.com/sthompsonjr/Endless-FxPatchSDK
```

---

## Community Forks

See [`../docs/endless-reference.md` § Community Forks](../docs/endless-reference.md#9-community-forks--active-repos)
for a full writeup of each repo.

| Fork | Description |
|---|---|
| [sthompsonjr/Endless-FxPatchSDK](https://github.com/sthompsonjr/Endless-FxPatchSDK) | Most developed — 9 effects, full WDF circuit modeling |
| [andybalham/FxPatchSDK](https://github.com/andybalham/FxPatchSDK) | Educational — Freeverb reverb, flanger, delay, distortion, clear structure |
| [rveitch/polyend-endless-effects](https://github.com/rveitch/polyend-endless-effects) | Standalone effects collection repo, early-stage |
