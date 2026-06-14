# JUCE VST3 / LV2 / Standalone Wrapper for Desktop Auditioning

**Status:** implemented in [`vst/`](../vst). This document is the design rationale;
[`vst/README.md`](../vst/README.md) is the build/use guide.

> **Experimental / test-only.** The wrapper is for desktop auditioning of
> patch logic, not for release, distribution, or live performance.
> Single-precision host floats and a different DSP toolchain mean voicing
> here will not be bit-identical to the pedal. Hardware listening remains
> the final word on any voicing decision; this is a sketchpad for fast
> iteration, not a master.

**Origin:** the `andybalham/FxPatchSDK` fork — see
[`docs/fork-comparisons/upstream-and-forks.md`](fork-comparisons/upstream-and-forks.md).

The build was verified on **Linux only** (Pop!_OS / Ubuntu 24.04, GCC 13): a
clean configure + build produces a `.vst3` bundle, an `.lv2` bundle, and a
Standalone app for the selected effect; the LV2 bundle is discovered and
parsed cleanly by `lv2ls` / `lv2info`; and reconfiguring with a different
`-DFX_EFFECT` produces a second plugin — confirming the one-effect-per-build
model. macOS and Windows builds should work in principle but are unverified
in this repo. The Linux build also needs JUCE's system-library set
(`libasound2-dev`, `libx11-dev`, `libxext-dev`, `libxrandr-dev`,
`libxinerama-dev`, `libxcursor-dev`, `libxrender-dev`, `libfreetype6-dev`,
`libgl1-mesa-dev`) — see [`vst/README.md`](../vst/README.md) for the apt
line. Loading in a DAW or MOD Audio Desktop, and running `pluginval`, still
needs a desktop environment with an audio device and display; that final
audition step is the user's to run.

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

## `vst/` layout

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
- `juce_add_plugin(... FORMATS VST3 LV2 Standalone ... LV2URI "urn:fxpatchsdk:${FX_EFFECT}")`
  — VST3 for DAWs, LV2 for MOD Audio Desktop and other LV2 hosts, Standalone for a
  no-DAW path. The LV2 URI is per-effect so several effects coexist. `PRODUCT_NAME` is
  kept space-free because it becomes the `.vst3` / `.lv2` bundle directory name.
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
| `handleAction(0/1)` | footswitch press/hold, driven two ways: the editor's momentary buttons push an int into a lock-free FIFO; and two `AudioParameterBool`s (`Footswitch Press` / `Footswitch Hold`) fire on their rising edge. Both are drained/checked at the top of `processBlock`. The parameter form is what a DAW or a MOD pedalboard maps a real footswitch to. |
| `getStateLedColor()` | polled by a `juce::Timer` for an LED indicator; map the 16-value `Color` enum through a lookup table |

Call order in `prepareToPlay()` mirrors `tests/effect_probe.cpp`: `setWorkingBuffer()` →
`init()` → re-push all three `setParamValue()` (because `init()` resets effect state).

## LV2 output and MOD Audio Desktop

The same build also emits an LV2 bundle, so an effect can run in MOD Audio Desktop
(and on the MOD Dwarf / Duo, and in other LV2 hosts). Design notes:

- JUCE 8 exposes plugin parameters through the LV2 `patch:` parameter extension
  (atom-message based), not legacy `lv2:ControlPort`s. MOD supports LV2 parameters
  from MOD OS v1.10 onward, which MOD Desktop is well past.
- The footswitch is exposed as two `AudioParameterBool`s precisely so it becomes a
  mappable parameter. MOD's pedalboard UI builds controls from parameters, not from
  the JUCE editor, so an editor-only button would be unreachable there. Firing on
  the rising edge means one momentary footswitch press maps to one `handleAction`.
- Each effect builds with a distinct LV2 URI (`urn:fxpatchsdk:<effect>`), and the
  bundle name is space-free, so installs into `~/.lv2` neither collide nor trip
  host-discovery bugs.
- MOD Desktop runs at 48 kHz over JACK by default, which matches the patch
  sample-rate assumption — the dry-passthrough fallback below does not engage.

## Sample rate and buffer contract

- Patches hard-code `Patch::kSampleRate = 48000` for every coefficient and delay length;
  there is no runtime-rate hook. **Recommended:** if `getSampleRate() != 48000`, resample
  the host block to 48 kHz around `processAudio` (a `juce::LagrangeInterpolator` per
  channel, ratio fixed in `prepareToPlay`), so the effect behaves as it will on hardware.
  Fast-path (bypass the resampler) when the host is already at 48 kHz, and tell the user
  in `vst/README.md` to set the DAW to 48 kHz. A reasonable first cut ships the 48 kHz
  fast-path only and treats the resampler as a follow-up.
- Declare a stereo-in/stereo-out main bus *and* reject any other layout in
  `isBusesLayoutSupported` so `processAudio` always gets independent L and R
  buffers. The corpus pattern `left[i] = processChannel(0, left[i], ...); right[i] = processChannel(1, right[i], ...);`
  would alias and overwrite if a mono layout shared one buffer, so mono is
  declined rather than papered over. Hosts on mono tracks adapt. JUCE
  `AudioBuffer` write pointers are already in-place and L/R are equal length,
  satisfying the SDK contract. Effects loop over `span.size()` and handle any
  block length.

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
   then `cmake --build vst/build`. Expect a `.vst3`, an `.lv2`, and a Standalone app.
   Validate the LV2 bundle: with `LV2_PATH` pointing at the build's `LV2/` directory,
   `lv2ls` should list `urn:fxpatchsdk:<effect>` and `lv2info` should load it cleanly.
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
- **LV2 parameter model.** JUCE 8 exposes parameters as LV2 `patch:` parameters, not
  control ports. This is standard and works in modern hosts, including MOD OS >= 1.10,
  but a very old LV2 host that only understands control ports would show no controls.
- **JUCE tag.** Pin to the latest verified JUCE 8 release tag at implementation time.
