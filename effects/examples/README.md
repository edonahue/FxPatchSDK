# Effect Examples

Reference implementations collected from community forks. Files kept here must compile
against the stock `polyend/FxPatchSDK` with no extra dependencies, because the goal of
this directory is study value, not dependency sprawl.

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

## Quick Control Cheat Sheet

| Surface | What it does |
| --- | --- |
| Left knob | `Room Size` (`0.6` default): shorter room feel at lower values, longer tail and bigger space as you turn it up. |
| Mid knob | `Damping` (`0.5` default): lower values keep more top-end in the tail, higher values darken the repeats faster. |
| Right knob | `Mix` (`0.33` default): blends from dry signal to full wet reverb. |
| Short press | Bypass toggle. |
| Long press | Same as short press; hold also toggles bypass. |
| Expression pedal | Mirrors the Right knob (`Mix`) through the repo-wide `param 2` routing. Heel stays mostly dry; toe moves toward full wet. |
| LED / colors | `kLightBlueColor` when active, `kDimWhite` when bypassed. There is no alternate long-press mode state. |

**To use:** copy to `source/PatchImpl.cpp`, change the include to `"Patch.h"`,
rename the class to `PatchImpl` (or leave it as `Reverb` — only `getInstance()`
needs to return the right type), then `make TOOLCHAIN=...`.

---

## What happened to the other examples?

Three additional effects (Optical Compressor, Tubescreamer, Wah) were sourced
from [sthompsonjr/Endless-FxPatchSDK](https://github.com/sthompsonjr/Endless-FxPatchSDK)
but removed because they depend on headers and SDK extensions not present in the
stock SDK used by this fork:

- `sdk/Patch.h` — sthompsonjr's modified SDK layout
- `wdf/Wdf*.h` — Wave Digital Filter circuit headers (25+ files)
- `dsp/ParameterSmoother.h`, `dsp/Saturation.h` — DSP utility headers
- `isParamEnabled(int, ParamSource)` — virtual method added to `Patch` base class

If you want those effects, clone sthompsonjr's repo directly — everything is there
and builds as a self-contained project:

```
git clone https://github.com/sthompsonjr/Endless-FxPatchSDK
```

That fork is still worth studying if you want to see where this repository could grow:
it extends the SDK, adds WDF infrastructure, and carries a much broader effect library.
