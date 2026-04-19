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

### Current wrapper exposes expression only on param `2`

`internal/PatchCppWrapper.cpp` enables:

- knobs for params `0`, `1`, `2`
- expression pedal only for param `2`

That means every patch in this repo currently treats the Right knob as the expression
pedal target whenever a pedal is connected.

Important distinction: the current Polyend Endless guide documents device-level
expression assignment to a chosen knob in pedal setup. This repo's current SDK wrapper
is narrower than the hardware and only exposes expression on `param 2`.

Implications:

- `chorus.cpp`: good fit, because Right is mix
- `mxr_distortion_plus.cpp`: good fit, because Right is output level
- `wah.cpp`: required, because Right is the sweep position
- future patches: be careful if Right is a mode selector or other set-and-forget control

There is a documented future path to a per-patch `isParamEnabled()` API, but it is not
implemented in this branch.

### `source/PatchImpl.cpp` is the active build target

In this fork, `source/PatchImpl.cpp` may temporarily mirror whichever custom effect is
currently being built or tested. Treat `effects/` as the source-of-truth patch library,
and treat `source/PatchImpl.cpp` as the active deployment target.

### The host-side test script is only a preflight

`tests/check_patches.sh` is good at catching syntax and basic SDK-rule violations, but it
does not replace a real ARM build or hardware listening pass.

---

## 6. Included Patch Inventory

### `effects/back_talk_reverse_delay.cpp`

- Back Talk-inspired reverse delay with chunk-based reverse playback
- uses the working buffer for stereo reverse capture and playback
- expression controls `Mix` via the repo-global param-2 routing
- demonstrates delay-buffer state management, feedback damping, and a hold-toggle texture mode

### `effects/bbe_sonic_stomp.cpp`

- guitar-oriented enhancer inspired by the BBE Sonic Stomp / Aion Lumin family
- uses a dry path plus low/mid/high correction deltas rather than static EQ only
- expression controls `Midrange` via the repo-global param-2 routing
- includes an optional long-press stereo doubler mode as a stretch feature

### `effects/big_muff.cpp`

- Ram's Head-inspired Big Muff fuzz with cascaded clipping and a Muff-style LP/HP tone blend
- uses expression on `Blend` instead of a literal output-volume control
- long-press toggles a Tone Bypass-inspired mids-lift alternate voice
- main case study for adapting a classic 3-knob pedal when param `2` must stay performance-friendly

### `effects/big_muff_wdf.cpp`

- hybrid WDF-style Big Muff sibling that keeps the same `Sustain` / `Tone` / `Blend` control surface
- replaces the original clip core with two wave-solved diode-pair stages plus the same general Muff LP/HP tone-stack story
- uses expression on `Blend` for an apples-to-apples comparison against `big_muff.cpp`
- main case study for asking whether WDF-style nonlinear stages improve feel enough to justify the extra math

### `effects/chorus.cpp`

- stock-SDK-compatible stereo chorus
- uses the working buffer for delay lines
- demonstrates fractional delay, LFO phase offset, and expression-as-mix

### `effects/harmonica.cpp`

- blues bullet-mic harmonica voicing built as a timbral transformation, not a pitch shifter
- uses dual Chamberlin formants, asymmetric reed-style saturation, post-rolloff, tremolo, and subtle micro-chorus
- expression controls `Waa / Cup sweep` via the repo-global param-2 routing
- hold toggles `Open` and `Cupped` voicings with different Q, drive, rolloff, and modulation depth

### `effects/klon_centaur.cpp`

- Klon-inspired transparent overdrive with clean/dirty summing and an active treble shelf
- preserves the classic `Gain` / `Treble` / `Output` control story
- expression controls `Output` via the repo-global param-2 routing
- long-press toggles a fuller Tone Mod alternate voice

### `effects/mxr_distortion_plus.cpp`

- MXR Distortion+ inspired distortion model, retuned for Endless usability
- maps a simple analog topology into HP -> gain -> `tanhf` -> LP -> level
- expression controls `Level` via the repo-global param-2 routing
- good example of coefficient precomputation outside the sample loop

### `effects/phase_90.cpp`

- one-knob Phase 90 phaser with a block voice by default and a script-mod vintage alternate
- keeps the center knob as the only public control and mirrors speed onto expression
- demonstrates a linked-stereo all-pass phaser with authentic minimal UI
- main case study for intentional unused controls and mode-driven voicing changes

### `effects/tube_screamer.cpp`

- TS808-inspired overdrive with a TS9 alternate voice on footswitch hold
- preserves the classic `Drive` / `Level` / `Tone` control story instead of replacing the third knob
- expression controls `Tone` via the repo-global param-2 routing
- main case study for bounded tone sweeps and family-relative overdrive voicing

### `effects/tube_screamer_wdf.cpp`

- WDF-style Tube Screamer sibling using a wave-solved diode-pair clip core and body/edge split before tone/output voicing
- intentionally remaps the surface to `Drive` / `Tone` / `Level`, with expression on `Level`, so the sibling can test a more pedal-like output story
- hold toggles TS808 and TS9 family voicings through a smoothed morph instead of a hard branch jump
- main case study for asking whether WDF-style clipping plus an output-focused Right knob is the better Endless interpretation of this pedal family

## 6.1 Control-Law Review Notes

This fork now treats knob taper review as part of patch design, not just polish.

- `mxr_distortion_plus.cpp`: primary control-law case study; the raw analog pot law was less usable than an Endless-tuned curve
- `big_muff.cpp`: main case study for passive tone-stack behavior, output compensation, and expression-as-blend
- `big_muff_wdf.cpp`: matching WDF-style sibling case; compare whether the wave-solved clip core changes sustain feel enough to matter while keeping the same public controls
- `klon_centaur.cpp`: main case study for gain-dependent clean/dirty summing and expression-as-output
- `phase_90.cpp`: main case study for one-knob authenticity, log-rate mapping, and intentional unused controls
- `tube_screamer.cpp`: main case study for preserving a classic control layout while making expression-on-tone musical
- `tube_screamer_wdf.cpp`: matching WDF-style sibling case; compare expression-on-level and a more circuit-shaped clip stage against the original hand-tuned build
- `back_talk_reverse_delay.cpp`: log-style time mapping, bounded repetitions, and equal-power expression-as-mix
- `chorus.cpp`: current mapping looks healthy; log taper for `Rate`, linear `Depth` and `Mix`
- `harmonica.cpp`: main case study for timbral transformation, coupled voicing trims, and log-formant expression mapping
- `wah.cpp`: current mapping looks healthy; log frequency sweep is the right model for wah motion
- `bbe_sonic_stomp.cpp`: current mapping is intentionally bounded and blend-oriented rather than analog-pot faithful

Current watch-items for future hardware listening:

- confirm `back_talk_reverse_delay.cpp` chunk edges stay click-free across extreme speed settings
- confirm `back_talk_reverse_delay.cpp` repetitions approach runaway gradually rather than abruptly
- confirm `big_muff.cpp` sustain rises musically across most of the sweep and the alternate mode is clearly more mid-forward than the core Ram's Head voice
- confirm `big_muff_wdf.cpp` preserves that same control usefulness while adding enough density or realism to justify the heavier clip core
- confirm `klon_centaur.cpp` lower gain settings stay open and stackable, and the Tone Mod hold-toggle feels fuller without turning the patch into a different pedal
- confirm `phase_90.cpp` speed stays useful across most of the sweep, the script mode is smoother than block mode, and both modes remain near unity
- confirm `tube_screamer.cpp` tone remains useful heel-to-toe under expression and the TS808/TS9 hold-toggle reads as a close family shift rather than a different pedal
- confirm `tube_screamer_wdf.cpp` level really behaves like a pedal output control and the TS808/TS9 morph is audible without turning the patch into "a louder EQ"
- confirm `chorus.cpp` depth is still useful near both extremes
- confirm `harmonica.cpp` reads as a hand-cupped bullet-mic voice on single-note lines, the Open/Cupped hold-toggle is obvious, and the expression sweep feels like hand motion rather than a synth-wah
- confirm `wah.cpp` Q remains musical across the full sweep
- confirm each `bbe_sonic_stomp.cpp` knob produces a distinct audible shift without bunching near the ends

### `effects/wah.cpp`

- dual-mode wah inspired by Crybaby and Vox behavior
- demonstrates a Chamberlin SVF, mode switching, and expression-driven control
- strong example of state clearing discipline around bypass/mode changes

### `effects/examples/reverb.cpp`

- imported reference example, not original to this fork
- useful for studying working-buffer allocation and larger delay-based structures

Related design notes:

- [`back-talk-reverse-delay-build-walkthrough.md`](back-talk-reverse-delay-build-walkthrough.md)
- [`bbe-sonic-stomp-research.md`](bbe-sonic-stomp-research.md)
- [`bbe-sonic-stomp-build-walkthrough.md`](bbe-sonic-stomp-build-walkthrough.md)
- [`big-muff-research.md`](big-muff-research.md)
- [`big-muff-build-walkthrough.md`](big-muff-build-walkthrough.md)
- [`big-muff-wdf-build-walkthrough.md`](big-muff-wdf-build-walkthrough.md)
- [`circuit-to-patch-conversion.md`](circuit-to-patch-conversion.md)
- [`harmonica-build-walkthrough.md`](harmonica-build-walkthrough.md)
- [`klon-centaur-research.md`](klon-centaur-research.md)
- [`klon-centaur-build-walkthrough.md`](klon-centaur-build-walkthrough.md)
- [`mxr-distortion-plus-circuit-analysis.md`](mxr-distortion-plus-circuit-analysis.md)
- [`phase-90-research.md`](phase-90-research.md)
- [`phase-90-build-walkthrough.md`](phase-90-build-walkthrough.md)
- [`tube-screamer-research.md`](tube-screamer-research.md)
- [`tube-screamer-build-walkthrough.md`](tube-screamer-build-walkthrough.md)
- [`tube-screamer-wdf-build-walkthrough.md`](tube-screamer-wdf-build-walkthrough.md)
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

Current recommended custom-patch workflow in this fork:

1. run `bash tests/check_patches.sh`
2. run `bash tests/build_effects.sh` for the real ARM build verification pass
3. run `bash tests/analyze_effects.sh` when you are retuning controls, gain staging, or expression behavior
4. or use `bash scripts/build_effects.sh` to generate deployable `.endl` files for all top-level `effects/*.cpp`
5. pick up the generated `.endl` files from `effects/builds/`
6. copy the chosen `.endl` onto the Endless USB volume

Legacy/manual single-patch path:

1. choose a file from `effects/` or write a new one there
2. copy it into `source/PatchImpl.cpp`
3. change `#include "../source/Patch.h"` to `#include "Patch.h"`
4. build with `make`

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

`playground/` is not source code. It is a reference archive of:

- compiled creator-made Playground effects and related PDFs
- the official Polyend Plates catalog documentation
- a local-only sync target for official Polyend Plates `.endl` downloads

These files are useful for:

- understanding the kind of effects people are getting from Playground
- understanding Polyend's own naming, categorization, and product-page presentation for Endless plates
- comparing UX and naming conventions against hand-written SDK patches
- building a listening/test corpus for future fork development

They are not useful for code extraction or line-by-line reverse engineering in the same
way that `effects/*.cpp` files are.

For the official Plates catalog and attribution notes, see
[`playground/polyend_plates/README.md`](../playground/polyend_plates/README.md). To sync
the current live catalog locally without committing the bulk archive, use
[`scripts/sync_polyend_plates.sh`](../scripts/sync_polyend_plates.sh).

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
