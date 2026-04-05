# Polyend Endless — Personal Reference

A single-page reference synthesizing hardware specs, SDK internals, build workflow, and community resources. Written for my own use while building custom patches.

---

## Table of Contents

1. [Hardware Quick Reference](#1-hardware-quick-reference)
2. [Playground — AI Patch Generator](#2-playground--ai-patch-generator)
3. [SDK Architecture](#3-sdk-architecture)
   - [The `Patch` C++ Class](#3a-the-patch-c-class)
   - [C ABI (`PatchABI.h`)](#3b-c-abi-patchabih)
   - [Build Output Format](#3c-build-output-format)
4. [Build System](#4-build-system)
5. [Hard Rules & Gotchas](#5-hard-rules--gotchas)
6. [Bitcrush Example Walkthrough](#6-bitcrush-example-walkthrough)
7. [Community & Resources](#7-community--resources)
8. [Keeping the Fork in Sync](#8-keeping-the-fork-in-sync)
9. [Community Forks & Active Repos](#9-community-forks--active-repos)

---

## 1. Hardware Quick Reference

| Property | Detail |
|---|---|
| **CPU** | ARM Cortex-M7 @ 720 MHz |
| **FPU** | fpv5-sp-d16 (single-precision hardware float) |
| **Audio** | Stereo, 48 kHz, 24-bit |
| **Audio I/O** | 2× 1/4" TRS in, 2× 1/4" TRS out |
| **Controls** | 3× multifunction knobs (Left, Mid, Right), 2× footswitches, 1× multicolor LED |
| **Expression** | 1× 1/4" TRS expression pedal input (freely assignable) |
| **USB** | 1× USB-C — power, data, drag-and-drop patch install |
| **MIDI** | None |
| **Power** | 9–12 V DC, ≥200 mA, center-neg or center-pos |
| **Size** | 4.72″ × 3.15″ × 2.2″ (H with knobs), 0.88 lbs |
| **Enclosure** | CNC-machined aluminum; magnetic swappable faceplates |
| **Faceplate** | Blank included; optional designs ~$20 each |
| **Price** | $299 USD / €299 EUR |

Signal path is configurable: Mono, Stereo, or Mono-to-Stereo.

---

## 2. Playground — AI Patch Generator

Playground is Polyend's hosted text-to-effect service — describe what you want in plain language, get a compiled `.endl` file back in minutes.

**Workflow:**
1. Type a natural-language prompt (e.g. "granular delay that smears into reverb")
2. Polyend's pipeline of specialized agents parses the request, selects DSP algorithms, generates C++ code, and runs automated tests
3. Download the ready-to-use `.endl` and drag it onto the Endless drive

**Token economy:**
- Pay-per-prompt (no subscription)
- Bundled: 2000 tokens ($20 value) with pedal purchase
- Cost per effect: ~$1–2 for simple effects, up to ~$5 for complex ones
- Access restricted to registered Endless owners

**SDK vs. Playground:**

| | SDK | Playground |
|---|---|---|
| Cost | Free, unlimited | ~$1–5 per effect |
| Control | Full C++ source | Black box |
| Speed | Build time + deploy | Minutes |
| Skill required | C++, DSP | Natural language |

Use the SDK for anything you want to iterate on, own, or understand.

---

## 3. SDK Architecture

The SDK is small: one abstract C++ class you implement, a thin C ABI layer between your code and the firmware, and a Makefile.

```
source/
  Patch.h          ← abstract base class; this is the public API
  PatchImpl.cpp    ← your effect code goes here

internal/
  PatchABI.h       ← C struct the firmware actually reads from the binary
  PatchCppWrapper.h/.cpp  ← bridges C++ Patch class → C ABI
  patch_main.c     ← entry point; emits the PatchHeader at binary start
  patch_imx.ld     ← linker script; positions binary at 0x80000000
```

### 3a. The `Patch` C++ Class

**File:** `source/Patch.h`

#### Constants

```cpp
static constexpr int kWorkingBufferSize = 2400000; // floats (~9.6 MB)
static constexpr int kSampleRate = 48000;
```

#### Methods you must implement

| Method | When called | Notes |
|---|---|---|
| `init()` | Once at patch load | Setup, zero state |
| `setWorkingBuffer(span<float, 2400000>)` | Once after init | Store the span if you need delay lines etc. in external RAM |
| `processAudio(span<float> left, span<float> right)` | Every audio frame | Hot path — keep it fast |
| `getParameterMetadata(int paramIdx)` | At load per param | Return `{minValue, maxValue, defaultValue}` |
| `setParamValue(int paramIdx, float value)` | On knob change | Called from audio thread; no sync needed |
| `handleAction(int actionIdx)` | On footswitch event | Toggle state, reset, etc. |
| `getStateLedColor()` | Periodically polled | Return a `Color` enum value |

#### Parameters

```cpp
namespace endless {
    enum class ParamId {
        kParamLeft,   // 0 — left knob
        kParamMid,    // 1 — middle knob
        kParamRight,  // 2 — right knob
    };
}
```

`getParameterMetadata` and `setParamValue` receive the raw int index (0/1/2). Guard with `if (idx == 0)` etc.

#### Actions

```cpp
namespace endless {
    enum class ActionId {
        kLeftFootSwitchPress,  // 0 — momentary press
        kLeftFootSwitchHold,   // 1 — held down
    };
}
```

#### LED Colors

16 options in `Patch::Color`:

```
kDimWhite    kDarkRed      kDarkLime     kDarkCobalt
kLightYellow kDimBlue      kBeige        kDimCyan
kMagenta     kLightBlueColor kPastelGreen kDimYellow
kBlue        kLightGreen   kRed          kDimGreen
```

`getStateLedColor()` is polled by the firmware — just return the right color based on your internal state. No push mechanism.

#### Minimal skeleton

```cpp
#include "Patch.h"

class PatchImpl : public Patch {
public:
    void init() override {}

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override {
        workingBuf = buf;
    }

    void processAudio(std::span<float> left, std::span<float> right) override {
        // process left and right in lockstep
    }

    ParameterMetadata getParameterMetadata(int /*paramIdx*/) override {
        return {0.0f, 1.0f, 0.5f};
    }

    void setParamValue(int idx, float value) override {
        if (idx == 0) param0 = value;
    }

    void handleAction(int /*idx*/) override {
        active = !active;
    }

    Color getStateLedColor() override {
        return active ? Color::kBlue : Color::kDimBlue;
    }

private:
    std::span<float, kWorkingBufferSize> workingBuf;
    float param0 = 0.5f;
    bool active = false;
};

static PatchImpl patch;
Patch* Patch::getInstance() { return &patch; }
```

---

### 3b. C ABI (`PatchABI.h`)

**File:** `internal/PatchABI.h`

The firmware doesn't call your C++ class directly — it reads a `PatchHeader` struct from the very beginning of the binary, which contains function pointers that the `PatchCppWrapper` layer fills in.

Key values:

| Symbol | Value | Meaning |
|---|---|---|
| `PATCH_MAGIC` | `0x48435450` (`'PTCH'`) | Header sanity check |
| `PATCH_ABI_VERSION` | `0x000B` | Bump = breaking firmware change |
| Load address | `0x80000000` | External RAM on the MCU |

`PatchHeader` includes: magic, abi_version, flags, init function pointer, a suite of `agent_*` function pointers (audio update, param get/set, action, LED state), `image_size`, `bss_begin`, `bss_size`, and 16 reserved words.

You never touch `PatchHeader` directly — `patch_main.c` and `PatchCppWrapper.cpp` handle it. Just note that:
- `agent_get_param_name` and `agent_get_param_unit` exist in the ABI but the high-level `Patch` C++ API doesn't expose a way to set them yet. Knob labels are not user-configurable from the SDK today.
- All `agent_*` pointers are optional (may be NULL) — the firmware handles missing ones gracefully.

---

### 3c. Build Output Format

| Property | Detail |
|---|---|
| Extension | `.endl` |
| Format | Raw ARM binary (not ELF) |
| Filename | `build/<PATCH_NAME>_YYYYMMDD_HHMMSS.endl` |
| Default name | `patch` (override with `PATCH_NAME=`) |

The binary is a flat image: `PatchHeader` at offset 0, followed by code and initialized data. BSS is cleared by the firmware loader using the `bss_begin`/`bss_size` fields in the header.

---

## 4. Build System

**File:** `Makefile`

### Requirements

- [GNU Arm Embedded Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain) — provides `arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-objcopy`

### Commands

```bash
# Build (Linux/macOS)
make TOOLCHAIN=/usr/bin/arm-none-eabi-

# Build with a custom patch name
make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=my_chorus

# Clean
make clean
```

Output: `build/<PATCH_NAME>_YYYYMMDD_HHMMSS.endl`

### Key compiler flags

| Flag | Purpose |
|---|---|
| `-mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard` | Target hardware |
| `-mthumb` | Thumb instruction set |
| `-std=c++20` | C++20 for `std::span`, etc. |
| `-fno-exceptions -fno-rtti` | Required — no C++ exceptions or RTTI |
| `-O3` | Full optimization |
| `-fsingle-precision-constant` | Float literals are `float`, not `double` |
| `-Wdouble-promotion` | Warns if a `float` gets silently widened to `double` |

### Deploy

Connect Endless via USB-C → drag-and-drop the `.endl` file onto the Endless drive → it loads automatically.

### VSCode

`.vscode/tasks.json` has pre-configured **Build** and **Build + Deploy** tasks. Set your toolchain path there.

---

## 5. Hard Rules & Gotchas

**Memory**
- No `malloc`, no `new`, no `std::vector`, no dynamic allocation of any kind. All state lives as members of your `PatchImpl` class, or in the working buffer provided via `setWorkingBuffer`.
- The working buffer (2.4M floats) lives in external RAM — reads/writes have higher latency than internal SRAM. Fine for initialization and large delay lines; avoid tight inner-loop random access.

**Audio**
- Output **must stay in `(-1.0f, 1.0f)`**. Hard clipping at the DAC. No automatic limiting.
- `processAudio()` must return before the next frame deadline. If it takes too long, frames drop and you'll hear glitches. Keep the hot path lean.
- `left` and `right` spans are guaranteed to be the same size. You can use a single index counter for both.

**Parameters**
- `setParamValue` is called for any knob that changes — not just the one you care about. Always check `idx` before storing.
- `getParameterMetadata` is called for all three params (indices 0, 1, 2). You can return different ranges per knob.
- All three knobs are always active from the user's perspective — even if your effect only uses one. Consider whether unused knobs should do something (or at least not cause confusion).

**Actions**
- Both `kLeftFootSwitchPress` (0) and `kLeftFootSwitchHold` (1) fire `handleAction`. If you only need press, just ignore idx 1.

**LED**
- `getStateLedColor()` is polled, not event-driven. Don't rely on it for timing. Just reflect your current state.

**C++ restrictions**
- No exceptions (`-fno-exceptions`), no RTTI (`-fno-rtti`). Don't use `dynamic_cast`, `typeid`, `throw`, or `try/catch`.
- Single-precision only (`-fsingle-precision-constant`, `-Wdouble-promotion`). Write `0.5f` not `0.5`. Math functions from `<cmath>` are fine — just watch for overloads that promote to double.

---

## 6. Bitcrush Example Walkthrough

**File:** `source/PatchImpl.cpp`

The default example is a bit-depth reduction (bitcrusher). Here's what each part does:

```cpp
void processAudio(std::span<float> audioBufferLeft, std::span<float> audioBufferRight) override
{
    // Map paramValue (0.0–1.0) to a quantization step count.
    // At paramValue=0: 2^(0*4+5) = 2^5 = 32 levels (heavy crush)
    // At paramValue=1: 2^(1*4+5) = 2^9 = 512 levels (subtle)
    auto values = std::pow(2.0f, (1.0f - paramValue) * 4.0f + 5.0f);
    auto valuesInv = 1.0f / values;

    for (auto leftIt = audioBufferLeft.begin(), rightIt = audioBufferRight.begin();
         leftIt != audioBufferLeft.end();
         ++leftIt, ++rightIt)
    {
        // Quantize: round to nearest step, scale back to (-1, 1)
        *leftIt  = std::round(*leftIt  * values) * valuesInv;
        *rightIt = std::round(*rightIt * values) * valuesInv;
    }
}
```

```cpp
ParameterMetadata getParameterMetadata(int /* paramIdx */) override {
    // All three knobs return the same range — only knob 0 is actually used
    return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
}
```

```cpp
void setParamValue(int idx, float value) override {
    if (idx == 0) {         // only store knob 0
        paramValue = value;
    }
    // idx 1, 2 silently ignored
}
```

```cpp
void handleAction(int /* idx */) override { state = !state; }
// Both footswitch press and hold toggle the state flag.
// The state flag doesn't actually change the audio in this example —
// it's just reflected in the LED color below.

Color getStateLedColor() override { return state ? Color::kBlue : Color::kDimBlue; }
```

**Key pattern to take from this example:**
- Pre-compute constants outside the sample loop (the `pow` is expensive — done once per buffer, not per sample)
- Use iterator-based loops over the spans
- Guard `setParamValue` by `idx`
- Reflect boolean state in LED color

---

## 7. Community & Resources

| Resource | URL |
|---|---|
| Product page | https://polyend.com/endless/ |
| Downloads (manual, quick start) | https://polyend.com/downloads/endless-downloads/ |
| Playground | https://polyend.com/playground/ |
| GitHub org | https://github.com/polyend |
| FxPatchSDK repo (upstream) | https://github.com/polyend/FxPatchSDK |
| Backstage forums (official community) | https://backstage.polyend.com/ |
| Dev setup tutorial on Backstage | https://backstage.polyend.com/t/getting-setup-to-develop-for-the-polyend-endless/24364 |
| MOD Wiggler thread | https://www.modwiggler.com/forum/viewtopic.php?t=298499 |

The Backstage forum has an Endless category with a growing library of community-built effects (examples seen: Multidrive, granular reverb, tape scanner, micro-looping arpeggiator). Community effects are free to download and share.

---

## 8. Keeping the Fork in Sync

This repo is a fork of `polyend/FxPatchSDK`. When Polyend updates the upstream SDK (new ABI features, build fixes, etc.), here's how to pull those in.

### One-time setup

```bash
git remote add upstream https://github.com/polyend/FxPatchSDK.git
git remote -v  # verify: should show both origin and upstream
```

### Syncing

```bash
# Fetch latest from upstream (no merge yet)
git fetch upstream

# Switch to your main branch
git checkout master

# Option A: rebase your master on top of upstream (cleaner history)
git rebase upstream/master

# Option B: merge (preserves merge commit, easier conflict resolution)
git merge upstream/master

# Push your updated master
git push origin master
```

### Conflict hotspot

`source/PatchImpl.cpp` is where your custom patches live, and it's also the file upstream ships as an example. It **will** conflict if upstream updates the example. Resolution is usually straightforward — keep your implementation, discard theirs.

`internal/` files (`PatchABI.h`, `PatchCppWrapper.*`, `patch_main.c`, `patch_imx.ld`) are infrastructure you don't touch. Take upstream's version if there's a conflict there, unless you've deliberately patched them.

### ABI version bumps

If `PATCH_ABI_VERSION` in `internal/PatchABI.h` changes in upstream, it's a **breaking change**. Old `.endl` binaries won't load on new firmware. Rebuild all your patches after merging.

### Checking for upstream changes

```bash
git fetch upstream
git log master..upstream/master --oneline  # see what's new upstream
git diff master upstream/master            # full diff
```

---

## 9. Community Forks & Active Repos

As of April 2026, there are 4 public forks of `polyend/FxPatchSDK` plus one standalone
effects collection. Here's what's actually in each one.

---

### sthompsonjr/Endless-FxPatchSDK
**https://github.com/sthompsonjr/Endless-FxPatchSDK**  
**Last push:** April 2, 2026 · ~342 KB · C++ (96.8%) · 44 commits · 1 star

The most developed fork by a significant margin. Added a full WDF (Wave Digital Filter)
library for analog circuit modeling, a `dsp/` utilities layer (parameter smoothing, saturation,
envelope followers), and extended the `Patch` base class with `isParamEnabled(int, ParamSource)`
to support expression pedal routing per-parameter.

**Effects implemented (9):**

| Effect | What it is |
|---|---|
| Optical Compressor | PC-2A, Optical Leveler, Hybrid Opt/OTA — three circuits, one right-knob selector |
| Tubescreamer Family | TS808, TS9, Klon Centaur clip stage — drive/tone/circuit select |
| Cry Baby Wah | Expression, auto-wah, LFO, EnvLfo modes; tap tempo via hold footswitch |
| Big Muff Pi | RamsHead, CivilWar, Triangle variants |
| ProCo Rat | 5 circuit variants |
| DOD 250 | Vintage overdrive with boost stage |
| Dallas Rangemaster | Treble booster with germanium transistor emulation |
| Yamaha SPX500 Shimmer | Grain-based pitch-shifted reverb |
| Bitcrush | Bit-depth and sample-rate reduction |

**Architecture notes:**
- 25+ WDF primitives (`wdf/` directory) for authentic analog modeling
- `sdk/Patch.h` is a superset of the stock SDK — adds `isParamEnabled` and `ParamSource` enum
- Effects live in `effects/` directory (separate from `source/`) as individual `.cpp` files
- Has a test suite

**Reference copies of 3 effects from this fork are in `effects/examples/`.**

---

### andybalham/FxPatchSDK
**https://github.com/andybalham/FxPatchSDK**  
**Last push:** March 22, 2026 · ~37 KB · C++ (76.7%) · fork of polyend/FxPatchSDK

Educational fork with clean, well-documented effect implementations. Effects are
header-only classes in `source/effects/`, selected via compile-time `#define`. Compiles
against the stock SDK with no extra dependencies — the best starting point if you want
to lift a working implementation straight into your own repo.

**Effects implemented (6):**

| Effect | What it is |
|---|---|
| Reverb | Freeverb (4 comb + 2 all-pass filters); delay lines in working buffer |
| Flanger | LFO-modulated ring buffer with linear interpolation |
| Delay | Echo with feedback and wet/dry mix |
| Distortion | Basic hard-clipping distortion |
| Saturation | Soft clipping |
| Bitcrush | Bit reduction and sample-rate decimation |

**Architecture notes:**
- Each effect is a header file defining a standalone class (no WDF dependencies)
- Effect selection is a compile-time `#define` in a config header — one binary per effect
- Very readable code; comments explain the algorithm, not just the code
- Working buffer usage is well-commented (see reverb as the best example)

**Reference copy of `reverb.cpp` from this fork is in `effects/examples/`.**

---

### rveitch/polyend-endless-effects
**https://github.com/rveitch/polyend-endless-effects**  
**Not a fork of FxPatchSDK** — standalone repo

Early-stage collection repo. Has the SDK vendored in as a subdirectory (`example/`)
alongside a top-level effects directory, suggesting intent to host multiple effects as
separate projects. At time of research the effects directory was sparse and undocumented.
Worth watching but not yet a useful reference.

---

### scarlton/FxPatchSDK
**https://github.com/scarlton/FxPatchSDK**  
**Last push:** January 23, 2026 (initial commit only) · 9 KB

Straight clone of the upstream repo at beta launch, no changes. Not useful as a reference.

---

### klausmobi32/FxPatchSDK
**https://github.com/klausmobi32/FxPatchSDK**  
Despite the name and description ("Create custom effects for the Polyend Endless pedal"),
the repo topics and contents are unrelated to audio (Kubernetes, WeChat, proxy tooling).
Skip.

---

### Example files

The most instructive patch files from the forks above are collected in
[`../effects/examples/`](../effects/examples/) with attribution headers and compilation notes.
