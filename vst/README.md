# VST3 / LV2 / Standalone Wrapper

> **Status: experimental / test-only.** This wrapper exists to audition
> patch logic on the desktop before flashing to Endless hardware. It is
> not a release artifact, not a substitute for hardware listening, and
> not intended for live performance or distribution. Single-precision
> host floats and a different DSP toolchain mean voicing on the desktop
> will not be bit-identical to the pedal — treat the wrapper as a
> sketchpad, not a master. Hardware listening remains the final word on
> any voicing decision.

Builds one FxPatchSDK effect as a JUCE plugin so it can be auditioned on the
desktop before flashing to Endless hardware. One build produces three formats:

- **VST3** — for DAWs (Reaper, Bitwig, Ardour, ...).
- **LV2** — for LV2 hosts, including **MOD Audio Desktop**.
- **Standalone** — a self-contained app, no DAW needed.

Design and rationale: [`../docs/vst-host-plan.md`](../docs/vst-host-plan.md).
This build is fully separate from the ARM firmware build (the root `Makefile`);
it references `source/` and `effects/` read-only.

## Platform support

The wrapper has been built and exercised on **Linux only** (Pop!_OS / Ubuntu
24.04, GCC 13). JUCE itself supports macOS and Windows, and the CMake setup
here uses no platform-specific code, so a macOS or Windows build should
work in principle — but neither has been verified by this repo. If you
build elsewhere, expect to hit JUCE's normal per-platform setup (Xcode
command-line tools on macOS, MSVC + the Windows SDK on Windows) on top of
the requirements below.

## Requirements

- CMake >= 3.22 and a C++20 host compiler.
- Network access on the first configure — JUCE is downloaded via CMake
  `FetchContent` (pinned to a release tag) and is never committed to this repo.
- **JUCE's Linux platform build dependencies.** The build will fail at
  configure or link time without these system packages. On Debian / Ubuntu
  / Pop!_OS:
  ```
  sudo apt install build-essential cmake git \
    libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxrender-dev libfreetype6-dev libgl1-mesa-dev
  ```
  Equivalent packages on Fedora / Arch / openSUSE exist under different
  names; JUCE's own documentation lists them.

## Build

```
cmake -S vst -B vst/build -DFX_EFFECT=tube_screamer -DCMAKE_BUILD_TYPE=Release
cmake --build vst/build
```

`FX_EFFECT` is the basename of any file in `effects/` (default `tube_screamer`).
One effect per build; reconfigure for another:

```
cmake -S vst -B vst/build -DFX_EFFECT=chorus
cmake --build vst/build
```

Output lands under `vst/build/FxPatchVST_artefacts/Release/`:

- `VST3/FxPatch_<effect>.vst3`
- `LV2/FxPatch_<effect>.lv2`
- `Standalone/FxPatch_<effect>`

(Bundle names are deliberately space-free — spaces in an LV2 bundle path break
discovery in some hosts.)

## Controls

All three formats expose the same controls:

- **Left / Mid / Right** — the effect's three knob parameters. Automatable. In
  this fork the expression pedal is wired to param 2, so automating **Right**
  is the expression-pedal stand-in.
- **Footswitch Press** / **Footswitch Hold** — the pedal footswitch. It is
  exposed two ways: as momentary buttons in the plugin's own editor window, and
  as toggle parameters (`handleAction` fires on the rising edge — one press,
  one action). The parameter form is what lets a DAW or a MOD pedalboard map a
  real footswitch to it. Press is usually bypass; Hold is usually the alternate
  voicing.
- The dot at the top-right of the editor mirrors the patch state LED.

## Use in a DAW (VST3)

```
mkdir -p ~/.vst3
cp -r vst/build/FxPatchVST_artefacts/Release/VST3/FxPatch_tube_screamer.vst3 ~/.vst3/
```

Set the project sample rate to 48 kHz and drop the plugin on a track.

## Use in MOD Audio Desktop (LV2)

MOD Audio Desktop — and the MOD Dwarf / Duo hardware — host **LV2** plugins.
Install the LV2 bundle where MOD scans for plugins:

```
mkdir -p ~/.lv2
cp -r vst/build/FxPatchVST_artefacts/Release/LV2/FxPatch_tube_screamer.lv2 ~/.lv2/
```

Restart MOD Desktop (or rescan plugins). The effect appears as a plugin you can
drop onto a pedalboard, exposing five parameters — Left, Mid, Right, Footswitch
Press, Footswitch Hold. The two footswitch parameters are boolean/toggled, so
you can assign a hardware or on-screen momentary footswitch to each.

Notes specific to MOD:

- MOD Desktop runs its engine at 48 kHz over JACK by default — exactly what the
  patches expect, so the sample-rate caveat below does not bite here.
- The wrapper exposes parameters through the LV2 `patch:` parameter extension
  (the modern JUCE LV2 export). MOD supports LV2 parameters from MOD OS v1.10
  onward, and MOD Desktop is well past that.
- Each effect builds with a distinct LV2 URI (`urn:fxpatchsdk:<effect>`), so
  several effects can be installed in `~/.lv2` side by side.
- MOD's pedalboard UI builds its own controls from the plugin's parameters; the
  JUCE editor window is not used in that view. That is why the footswitch is a
  parameter, not only an editor button.

## Limitations

- **Run the host at 48 kHz.** Patches assume `Patch::kSampleRate = 48000`. At
  any other rate the wrapper passes audio through dry and shows a warning
  rather than producing wrong-sounding output. (MOD Desktop defaults to 48 kHz;
  in a DAW, set the project rate.) Resampling is a documented future option in
  [`../docs/vst-host-plan.md`](../docs/vst-host-plan.md).
- **One instance at a time.** The SDK exposes the patch as a singleton
  (`Patch::getInstance()`), so two plugin instances would share one effect.
  Load a single instance.
- **Not a hardware substitute.** Host floats are not Cortex-M7 floats; this is
  for fast iteration, not final sign-off. Hardware listening still decides.
- **Exceptions/RTTI are enabled here**, unlike the firmware build, because JUCE
  requires them. The host probe (`scripts/analyze_effects.py`) makes the same
  compromise, so behavior parity with the probe holds.

## Licensing

JUCE is multi-licensed (AGPLv3 or commercial). This wrapper is intended as a
local, non-distributed audition tool. Do not redistribute the built plugin
without satisfying JUCE's license terms. JUCE is fetched at build time and is
never vendored into this repository.
