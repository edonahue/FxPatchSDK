# Polyend Endless / FxPatchSDK Developer Reference

This document is the current technical reference for working in this fork. It combines
verified official product information with repo-local behavior that matters for patch
development.

For the dated branch audit and codebase review that informed this document, see
[`repository-review.md`](repository-review.md).

---

## 1. Official Context

Polyend markets Endless as a programmable effects pedal with two parallel creation paths:

- write C++ patches against the official `polyend/FxPatchSDK`
- generate compiled patches through the hosted `Playground` platform

Useful official entry points:

| Resource | URL | Why it matters |
|---|---|---|
| Endless product page | <https://polyend.com/endless/> | hardware and product positioning |
| Playground | <https://polyend.com/playground/> | hosted patch-generation workflow |
| Endless downloads | <https://polyend.com/downloads/endless-downloads/> | manual and quick-start references |
| Upstream SDK | <https://github.com/polyend/FxPatchSDK> | baseline repo structure and public API |
| Backstage | <https://backstage.polyend.com/> | official community and setup posts |

What matters for this fork:

- Endless exposes a small, fixed control surface to patch code: three parameters,
  one exposed footswitch with press/hold events, and a state LED.
- The official SDK is intentionally minimal. Most product behavior lives in the device
  firmware, not in the repo.
- Playground gives you compiled `.endl` outputs, but not the editable source that a
  normal SDK workflow gives you.

---

## 2. This Fork at a Glance

This fork keeps the stock SDK layout, then adds practical development layers on top:

```text
source/        upstream-style public API and default example patch
internal/      ABI bridge used by the firmware loader
effects/       custom patches that compile against the stock SDK
tests/         host-side syntax/lint checks for effects/*.cpp
docs/          design notes, walkthroughs, repo review, SDK reference
playground/    downloaded example artifacts from Polyend Playground creators
```

For current branch and fork-history conclusions, use
[`repository-review.md`](repository-review.md) as the source of truth. This document is
the technical reference, not the dated branch-status narrative.

---

## 3. Hardware and Platform Notes

The official product page and Endless materials describe the pedal as a stereo effects
platform with USB-based patch loading and two creation modes. The repo itself confirms
or constrains the developer-facing subset:

| Area | What matters in code |
|---|---|
| Sample rate | `Patch::kSampleRate = 48000` |
| Audio buffers | stereo spans of equal length |
| Parameter count | `endless::kParams = 3` |
| Actions | `kLeftFootSwitchPress`, `kLeftFootSwitchHold` |
| LED | `Patch::Color` enum in `source/Patch.h` |
| Working buffer | `2400000` floats, provided once via `setWorkingBuffer()` |

Repo-local note: the hardware has more physical controls than the SDK currently exposes
to patch code. In practice, you program against the public API, not against every pedal
surface directly.

---

## 4. Patch API and ABI Shape

### `source/Patch.h`

The public interface is one abstract base class:

- `init()`
- `setWorkingBuffer(std::span<float, kWorkingBufferSize>)`
- `processAudio(std::span<float> left, std::span<float> right)`
- `getParameterMetadata(int paramIdx)`
- `setParamValue(int paramIdx, float value)`
- `handleAction(int actionIdx)`
- `getStateLedColor()`

Important constants:

```cpp
static constexpr int kWorkingBufferSize = 2400000;
static constexpr int kSampleRate = 48000;
```

Parameter indices are raw `int` values:

- `0`: Left knob
- `1`: Mid knob
- `2`: Right knob

### `internal/PatchABI.h`

The device firmware loads a `PatchHeader` from the beginning of the generated image.
Important fields and values in the current repo:

- `PATCH_MAGIC = 0x48435450u` (`PTCH`)
- `PATCH_ABI_VERSION = 0x000Bu`
- fixed load address configured through the linker script and `PATCH_LOAD_ADDR`

You normally do not edit the ABI layer directly. The path is:

`Patch` implementation -> `PatchCppWrapper.cpp` -> `PatchHeader` function pointers

### Build artifact

The Makefile emits a raw ARM image with an `.endl` extension:

`build/<PATCH_NAME>_<YYYYMMDD_HHMMSS>.endl`

This is not an ELF deployment artifact. `objcopy` flattens the linked binary before
it is copied onto the pedal.

---

## 5. Repo-Local Behavior That Will Surprise You

### Expression pedal routing is hardcoded

`internal/PatchCppWrapper.cpp` enables:

- knobs for params `0`, `1`, `2`
- expression pedal only for param `2`

That means every patch in this repo currently treats the Right knob as the expression
pedal target whenever a pedal is connected.

Implications:

- `chorus.cpp`: good fit, because Right is mix
- `mxr_distortion_plus.cpp`: good fit, because Right is output level
- `wah.cpp`: required, because Right is the sweep position
- future patches: be careful if Right is a mode selector or other set-and-forget control

There is a documented future path to a per-patch `isParamEnabled()` API, but it is not
implemented in this branch.

### `source/PatchImpl.cpp` is still upstream-style

The default file in `source/` is a simple bitcrusher example. It is useful as a minimal
API reference, but it is not the most interesting patch work in this fork. For real
development patterns, read files in `effects/`.

### The host-side test script is only a preflight

`tests/check_patches.sh` is good at catching syntax and basic SDK-rule violations, but it
does not replace a real ARM build or hardware listening pass.

---

## 6. Included Patch Inventory

### `effects/bbe_sonic_stomp.cpp`

- guitar-oriented enhancer inspired by the BBE Sonic Stomp / Aion Lumin family
- uses a dry path plus low/mid/high correction deltas rather than static EQ only
- expression controls `Midrange` via the repo-global param-2 routing
- includes an optional long-press stereo doubler mode as a stretch feature

### `effects/chorus.cpp`

- stock-SDK-compatible stereo chorus
- uses the working buffer for delay lines
- demonstrates fractional delay, LFO phase offset, and expression-as-mix

### `effects/mxr_distortion_plus.cpp`

- circuit-informed distortion model
- maps a simple analog topology into HP -> gain -> `tanhf` -> LP -> level
- good example of coefficient precomputation outside the sample loop

### `effects/wah.cpp`

- dual-mode wah inspired by Crybaby and Vox behavior
- demonstrates a Chamberlin SVF, mode switching, and expression-driven control
- strong example of state clearing discipline around bypass/mode changes

### `effects/examples/reverb.cpp`

- imported reference example, not original to this fork
- useful for studying working-buffer allocation and larger delay-based structures

Related design notes:

- [`bbe-sonic-stomp-research.md`](bbe-sonic-stomp-research.md)
- [`bbe-sonic-stomp-build-walkthrough.md`](bbe-sonic-stomp-build-walkthrough.md)
- [`circuit-to-patch-conversion.md`](circuit-to-patch-conversion.md)
- [`mxr-distortion-plus-circuit-analysis.md`](mxr-distortion-plus-circuit-analysis.md)
- [`wah-build-walkthrough.md`](wah-build-walkthrough.md)

---

## 7. Build Workflow

Baseline command:

```bash
make TOOLCHAIN=/usr/bin/arm-none-eabi-
```

Named build:

```bash
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=my_patch
```

Typical custom-patch workflow in this fork:

1. choose a file from `effects/` or write a new one there
2. copy it into `source/PatchImpl.cpp`
3. change `#include "../source/Patch.h"` to `#include "Patch.h"`
4. build with `make`
5. copy the resulting `.endl` onto the Endless USB volume

Useful flags in the current Makefile:

- `-mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard`
- `-std=c++20`
- `-fno-exceptions -fno-rtti`
- `-fsingle-precision-constant`
- `-Wdouble-promotion`
- `-O3`

---

## 8. Hard Rules for Patch Authors

- No heap use: no `new`, `malloc`, `std::vector`, or similar dynamic allocation.
- Keep output within `(-1.0f, 1.0f)`; there is no safety net in the SDK.
- Keep expensive work outside the inner sample loop whenever possible.
- Use `float` literals with `f` suffixes.
- Treat `setParamValue()` as audio-thread code.
- Expect `left` and `right` spans to be in-place buffers.
- Use the working buffer for large delay/state storage, not for convenience-only data.

These rules are enforced partly by toolchain flags and partly by discipline.

---

## 9. Playground Artifacts in This Repo

`playground/` is not source code. It is a reference archive of compiled creator-made
Playground effects and related PDFs. These files are useful for:

- understanding the kind of effects people are getting from Playground
- comparing UX and naming conventions against hand-written SDK patches
- building a listening/test corpus for future fork development

They are not useful for code extraction or line-by-line reverse engineering in the same
way that `effects/*.cpp` files are.

---

## 10. Syncing with Upstream

This repo is still structurally close enough to upstream that syncing remains realistic.

Recommended setup:

```bash
git remote add upstream https://github.com/polyend/FxPatchSDK.git
git fetch upstream
```

When reviewing an upstream update, pay closest attention to:

- `source/Patch.h`
- `internal/PatchABI.h`
- `internal/PatchCppWrapper.*`
- `internal/patch_main.c`
- `internal/patch_imx.ld`
- `Makefile`

Conflict pattern to expect:

- `source/PatchImpl.cpp` will often conflict trivially because upstream uses it as the
  default example slot and this fork uses it as a build target.
- `internal/` changes may indicate real firmware/ABI compatibility changes and should be
  treated carefully.

If upstream bumps `PATCH_ABI_VERSION`, rebuild every patch before trusting old `.endl`
artifacts.
