# Effect Examples

Reference implementations collected from community forks.
Each file here compiles against the stock `polyend/FxPatchSDK` with no extra dependencies.

---

## Files

| File | Effect | Source |
|---|---|---|
| [`reverb.cpp`](reverb.cpp) | Freeverb stereo reverb | andybalham/FxPatchSDK |

---

## reverb.cpp — Freeverb Stereo Reverb

**Source:** [andybalham/FxPatchSDK](https://github.com/andybalham/FxPatchSDK) · MIT License

Classic Schroeder/Freeverb algorithm. 4 parallel feedback comb filters pass into
2 series all-pass filters. Left and right channels use slightly different delay
lengths for stereo width. All delay buffers (~50 KB total) are sub-allocated from
the working buffer provided by `setWorkingBuffer()` — a good pattern to study for
any effect that needs large delay lines.

**Controls:**

| Knob | Parameter | Range | Default |
|---|---|---|---|
| Left | Room Size | 0–1 | 0.6 |
| Mid | Damping | 0–1 (0=bright, 1=dark) | 0.5 |
| Right | Dry/Wet Mix | 0–1 | 0.33 |

**Footswitch:** Press or hold → bypass toggle  
**LED:** `kLightBlueColor` = active · `kDimWhite` = bypassed

**To use:** copy to `source/PatchImpl.cpp`, change the include to `"Patch.h"`,
rename the class to `PatchImpl` (or leave it as `Reverb` — only `getInstance()`
needs to return the right type), then `make TOOLCHAIN=...`.

---

## What happened to the other examples?

Three additional effects (Optical Compressor, Tubescreamer, Wah) were sourced
from [sthompsonjr/Endless-FxPatchSDK](https://github.com/sthompsonjr/Endless-FxPatchSDK)
but removed because they depend on headers and SDK extensions not present in the
stock SDK:

- `sdk/Patch.h` — sthompsonjr's modified SDK layout
- `wdf/Wdf*.h` — Wave Digital Filter circuit headers (25+ files)
- `dsp/ParameterSmoother.h`, `dsp/Saturation.h` — DSP utility headers
- `isParamEnabled(int, ParamSource)` — virtual method added to `Patch` base class

If you want those effects, clone sthompsonjr's repo directly — everything is there
and builds as a self-contained project:

```
git clone https://github.com/sthompsonjr/Endless-FxPatchSDK
```

That fork has 9 effects total: Optical Compressor, Tubescreamer (TS808/TS9/Klon),
Cry Baby Wah, Big Muff Pi, ProCo Rat, DOD 250, Dallas Rangemaster, SPX500 Shimmer,
and Bitcrush. See [docs/endless-reference.md §9](../../docs/endless-reference.md)
for a full breakdown of all forks.
