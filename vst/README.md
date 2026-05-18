# VST3 / Standalone Wrapper

Builds one FxPatchSDK effect as a JUCE VST3 plugin (and a Standalone app) so it
can be auditioned on the desktop before flashing to Endless hardware.

Design and rationale: [`../docs/vst-host-plan.md`](../docs/vst-host-plan.md).
This build is fully separate from the ARM firmware build (the root `Makefile`);
it references `source/` and `effects/` read-only.

## Requirements

- CMake >= 3.22 and a C++20 host compiler.
- Network access on the first configure — JUCE is downloaded via CMake
  `FetchContent` (pinned to a release tag) and is never committed to this repo.
- JUCE's platform build dependencies. On Debian/Ubuntu:
  ```
  sudo apt install libasound2-dev libx11-dev libxext-dev libxrandr-dev \
                    libxinerama-dev libxcursor-dev libfreetype6-dev libgl1-mesa-dev
  ```

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

Output lands under `vst/build/FxPatchVST_artefacts/` — a `.vst3` bundle and a
Standalone app.

## Using it

- The three knobs map to the effect's Left / Mid / Right parameters and are
  automatable. In this fork the expression pedal is wired to param 2 (Right),
  so automating the Right knob is the expression-pedal stand-in.
- **Footswitch** triggers a press (`handleAction` 0, usually bypass);
  **Hold (alt voice)** triggers a hold (`handleAction` 1, usually a voice
  toggle).
- The dot at the top-right mirrors the patch state LED.

## Limitations

- **Run the host at 48 kHz.** Patches assume `Patch::kSampleRate = 48000`. At
  any other rate this wrapper passes audio through dry and shows a warning
  rather than producing wrong-sounding output. Resampling is a documented
  future option in [`../docs/vst-host-plan.md`](../docs/vst-host-plan.md).
- **One instance at a time.** The SDK exposes the patch as a singleton
  (`Patch::getInstance()`), so two plugin instances would share one effect.
  Load a single instance.
- **Not a hardware substitute.** Host floats are not Cortex-M7 floats; this is
  for fast iteration, not final sign-off. Hardware listening still decides.
- **Exceptions/RTTI are enabled here**, unlike the firmware build, because JUCE
  requires them. The host probe (`scripts/analyze_effects.py`) already makes
  the same compromise, so behavior parity with the probe holds.

## Licensing

JUCE is multi-licensed (AGPLv3 or commercial). This wrapper is intended as a
local, non-distributed audition tool. Do not redistribute the built plugin
without satisfying JUCE's license terms. JUCE is fetched at build time and is
never vendored into this repository.
