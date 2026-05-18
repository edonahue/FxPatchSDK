# JUCE VST3 Wrapper for Desktop Auditioning

**Status:** implemented in [`vst/`](../vst). This document is the design rationale;
[`vst/README.md`](../vst/README.md) is the build/use guide.
**Origin:** the `andybalham/FxPatchSDK` fork — see
[`docs/fork-comparisons/upstream-and-forks.md`](fork-comparisons/upstream-and-forks.md).

The build was verified on Linux: a clean configure + build produced both a `.vst3`
bundle and a Standalone app for `tube_screamer`, and reconfiguring with
`-DFX_EFFECT=chorus` produced a second plugin — confirming the one-effect-per-build
model. Loading in a DAW / running `pluginval` still needs a desktop environment with
an audio device and display; that final audition step is the user's to run.

## Why

This repo can syntax-check a patch ([`tests/check_patches.sh`](../tests/check_patches.sh)),
build a real `.endl` ([`tests/build_effects.sh`](../tests/build_effects.sh)), and run a
synthetic probe ([`tests/analyze_effects.sh`](../tests/analyze_effects.sh)) — but it
cannot *listen* to a patch without the Endless hardware. A JUCE VST3 (plus Standalone)
wrapper lets the exact same patch DSP run in a DAW, so control laws and voicing can be
auditioned before flashing. It does not replace hardware listening — host floats are not
Cortex-M7 floats — but it shortens the loop the same way the probe harness does.

## How the repo already compiles effects on the host

The wrapper reuses an existing model rather than inventing one. Two facts make it work:

- Every `effects/<name>.cpp` is a self-contained `Patch` subclass: it opens with
  `#include "../source/Patch.h"` and ends by defining the SDK's one extern symbol,
  `Patch* Patch::getInstance()`, returning a static instance. Compiling one effect TU
  is therefore a complete patch — `source/Patch.h` is header-only.
- The host probe already does exactly this. `scripts/analyze_effects.py` compiles
  `tests/effect_probe.cpp` + `effects/<name>.cpp` with host `g++`
  (`-std=c++20 -O2 -fsingle-precision-constant -I source`, no `sed` rewrite — the
  effect's relative `../source/Patch.h` include resolves against its own directory).

The VST plugin is structurally the same link: `PluginProcessor.cpp` + one
`effects/<name>.cpp`. `source/PatchImpl.cpp` must be excluded — it defines a second
`Patch::getInstance()` and would collide (the ARM `Makefile` filters it for the same
reason).

`-fno-exceptions` / `-fno-rtti` (firmware flags) are **not** used: JUCE needs both, and
the host probe already drops them. `-fsingle-precision-constant` is applied to the
effect TU only, so float-literal behavior still matches firmware.

## Proposed `vst/` layout

```
vst/
├── CMakeLists.txt          JUCE FetchContent, FX_EFFECT cache var, plugin target
├── README.md               build steps, FX_EFFECT usage, JUCE licensing note
└── src/
    ├── PluginProcessor.h    juce::AudioProcessor subclass — the wrapper
    ├── PluginProcessor.cpp
    ├── PluginEditor.h       minimal editor: 3 knobs, 2 footswitch buttons, LED
    ├── PluginEditor.cpp
    └── EffectConfig.h.in    configure_file template carrying FX_EFFECT name
```

`effects/<name>.cpp` and `source/Patch.h` are referenced in place, never copied. Nothing
under `vst/` shadows a repo file. The root `Makefile`, `internal/`, `source/`, and
`effects/` are untouched, so the ARM build cannot be affected.

## CMake structure

- `cmake_minimum_required(VERSION 3.22)`, C++20.
- `FX_EFFECT` cache variable (default e.g. `tube_screamer`) selects the effect, mirroring
  `scripts/build_effects.sh --effect <name>`. Validate `effects/${FX_EFFECT}.cpp` exists
  with an `EXISTS` check; fail clearly if not.
- JUCE via `FetchContent_Declare(juce GIT_REPOSITORY ... GIT_TAG <pinned-tag> GIT_SHALLOW TRUE)`
  — pin a specific JUCE 8 release tag, never a branch. JUCE is downloaded per-clone, never
  vendored.
- `juce_add_plugin(... FORMATS VST3 Standalone ...)` — Standalone gives a no-DAW audition
  path.
- `target_sources` = `PluginProcessor.cpp` + `PluginEditor.cpp` + `effects/${FX_EFFECT}.cpp`.
- `target_include_directories` += `source/` (parity with `-I source`) and the build dir
  (for generated `EffectConfig.h`).
- `set_source_files_properties(effects/${FX_EFFECT}.cpp ... COMPILE_OPTIONS -fsingle-precision-constant)`
  — effect TU only.
- Consider deriving a unique 4-char `PLUGIN_CODE` and product name from `FX_EFFECT` so a
  DAW can tell separate effect builds apart.

## AudioProcessor mapping

| SDK `Patch` member | JUCE side |
| --- | --- |
| `getInstance()` | held as a `Patch*` member of the processor |
| `setWorkingBuffer()` | a `std::vector<float>` of `kWorkingBufferSize` (9.6 MB), allocated in `prepareToPlay()`, never on the audio thread |
| `init()` | called in `prepareToPlay()` after `setWorkingBuffer()` |
| `getParameterMetadata(i)` | builds 3 `juce::AudioParameterFloat` from `{min,max,default}` |
| `setParamValue(i,v)` | called from `processBlock` when a parameter changed (SDK documents it as audio-thread-safe) |
| `processAudio(L,R)` | called from `processBlock` with `getWritePointer` spans (in-place, matches the SDK contract) |
| `handleAction(0/1)` | footswitch press/hold; editor buttons push an int into a lock-free FIFO drained at the top of `processBlock` (do not call from the UI thread) |
| `getStateLedColor()` | polled by a `juce::Timer` for an LED indicator; map the 16-value `Color` enum through a lookup table |

Call order in `prepareToPlay()` mirrors `tests/effect_probe.cpp`: `setWorkingBuffer()` →
`init()` → re-push all three `setParamValue()` (because `init()` resets effect state).

## Sample rate and buffer contract

- Patches hard-code `Patch::kSampleRate = 48000` for every coefficient and delay length;
  there is no runtime-rate hook. **Recommended:** if `getSampleRate() != 48000`, resample
  the host block to 48 kHz around `processAudio` (a `juce::LagrangeInterpolator` per
  channel, ratio fixed in `prepareToPlay`), so the effect behaves as it will on hardware.
  Fast-path (bypass the resampler) when the host is already at 48 kHz, and tell the user
  in `vst/README.md` to set the DAW to 48 kHz. A reasonable first cut ships the 48 kHz
  fast-path only and treats the resampler as a follow-up.
- Declare a stereo-in/stereo-out main bus so `processAudio` always gets an independent
  L and R (chorus and other stereo effects use separate L/R state). JUCE `AudioBuffer`
  write pointers are already in-place and L/R are equal length, satisfying the SDK
  contract. Effects loop over `span.size()` and handle any block length.

## `.gitignore` additions

```gitignore
# JUCE VST3 wrapper
vst/build/
vst/cmake-build-*/
vst/_deps/
```

JUCE itself is never committed; FetchContent re-downloads it per clone.

## Verification

1. `cmake -S vst -B vst/build -DFX_EFFECT=tube_screamer -DCMAKE_BUILD_TYPE=Release`
   then `cmake --build vst/build`. Expect a `.vst3` and a Standalone app.
2. Compare against the probe: run `bash tests/analyze_effects.sh` for the same effect;
   the VST at 48 kHz should match the probe's measured behavior.
3. Run JUCE `pluginval` (strictness 8–10) — it exercises random block sizes, sample
   rates, `prepareToPlay`/`releaseResources` cycling, and flags allocation in the audio
   callback (the 9.6 MB buffer must be allocated only in `prepareToPlay`).
4. Manual DAW audition: audio passes; 3 knobs sweep and automate; press/hold buttons
   toggle bypass/voice and move the LED.
5. Confirm the ARM build is untouched: `bash tests/check_patches.sh` and a normal
   `make` still succeed.
6. Reconfigure with `-DFX_EFFECT=chorus` to confirm the one-effect-per-build model.

## Risks and open questions

- **JUCE licensing.** JUCE 8 is multi-licensed (AGPLv3 or commercial). A locally built,
  non-distributed audition tool is fine, but the build must not be redistributed without
  resolving JUCE licensing, and the AGPL splash screen should be left enabled unless a
  JUCE license covers disabling it. `vst/README.md` must state this.
- **Exceptions/RTTI divergence.** The effect TU is compiled here with exceptions+RTTI
  enabled (JUCE requires them); firmware builds without. This is the same compromise the
  existing host probe already makes — accepted, but noted.
- **Undocumented thread contracts.** `handleAction()` and `getStateLedColor()` have no
  documented thread contract; the plan routes both through the audio thread (FIFO for
  actions, atomic snapshot for the LED). Fine for the current twelve effects, which only
  flip bools.
- **Resampler scope.** Open question whether non-48 kHz auditioning is needed at all. If
  not, drop the resampler entirely and just require a 48 kHz DAW project.
- **JUCE tag.** Pin to the latest verified JUCE 8 release tag at implementation time.
