# effects/

This directory is for custom patches built on top of the FxPatchSDK.

---

## Workflow

Each patch is a self-contained C++ implementation of the `Patch` abstract class.
To build and deploy one:

1. **Copy** the patch file to `source/PatchImpl.cpp` (replacing the existing file)
2. **Fix the include:** change `#include "../source/Patch.h"` → `#include "Patch.h"`
3. **Build:**
   ```bash
   make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=my_effect
   ```
4. **Deploy:** connect Endless via USB-C, copy `build/my_effect_<timestamp>.endl`
   to the Endless drive

Or use the VSCode **Build** / **Build + Deploy** tasks (`.vscode/tasks.json`).

---

## Structure

```
effects/
  README.md              ← you are here
  chorus.cpp             ← stereo chorus (first custom patch)
  mxr_distortion_plus.cpp ← MXR Distortion+ circuit model
  examples/              ← reference implementations from community forks
    README.md
    reverb.cpp           ← Freeverb stereo reverb (compiles against stock SDK)
```

Add your own patch files at the top level of this directory, e.g.:

```
effects/
  chorus.cpp
  fuzz.cpp
  delay.cpp
  examples/
    ...
```

---

## Quick patch template

```cpp
#include "Patch.h"

class PatchImpl : public Patch
{
public:
    void init() override {}

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
    {
        // Store buf if you need delay lines or large tables in external RAM.
        // Access is higher-latency than internal SRAM — fine for init/sparse access.
        (void)buf;
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        // Hot path — keep this fast. left.size() == right.size() always.
        for (size_t i = 0; i < left.size(); ++i)
        {
            left[i]  = left[i]  * gain_;
            right[i] = right[i] * gain_;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        // Called for paramIdx 0, 1, 2 (Left, Mid, Right knobs)
        (void)paramIdx;
        return {0.0f, 1.0f, 0.5f}; // min, max, default
    }

    void setParamValue(int idx, float value) override
    {
        // Called from audio thread when a knob changes. No sync needed.
        if (idx == 0) gain_ = value;
    }

    void handleAction(int idx) override
    {
        // idx 0 = left footswitch press, idx 1 = left footswitch hold
        if (idx == 0) active_ = !active_;
    }

    Color getStateLedColor() override
    {
        return active_ ? Color::kBlue : Color::kDimBlue;
    }

private:
    float gain_  = 0.5f;
    bool  active_ = true;
};

static PatchImpl patch;
Patch* Patch::getInstance() { return &patch; }
```

**Rules:**
- No heap (`malloc`, `new`, `std::vector`) — all state must be members or in the working buffer
- Output must stay in `(-1.0f, 1.0f)` — no automatic limiting
- No exceptions, no RTTI
- Write `0.5f` not `0.5` (single-precision only; `-Wdouble-promotion` will catch you)

---

## Expression pedal

**Current repo behaviour:** expression pedal is hardcoded to param 2 (Right knob)
for every patch. This is set in `internal/PatchCppWrapper.cpp`. Heel down = 0.0,
toe down = 1.0, same range as the knob. When the pedal is connected, the firmware
uses pedal values for param 2 and ignores the Right knob.

**Implication for patch design:**
- If your Right knob is a live-performance control (mix, level, depth) — expression
  pedal works automatically with no extra code.
- If your Right knob is a set-and-forget parameter (circuit selector, mode switch,
  etc.) — either swap which knob is your live-performance target, or change the
  `idx == 2` condition in `PatchCppWrapper.cpp` to match.

**Future: per-patch control.** The clean solution is a virtual `isParamEnabled`
method on `Patch.h` so each patch declares its own expression pedal routing without
touching shared infrastructure. See `docs/endless-reference.md §3a` for the full
design and code snippet.

See [`docs/endless-reference.md`](../docs/endless-reference.md) for the full SDK reference.

---

## Circuit-based patch design

Custom patches in this repository are built from analog circuit analysis.

- [`docs/mxr-distortion-plus-circuit-analysis.md`](../docs/mxr-distortion-plus-circuit-analysis.md) —
  Component-level analysis of the MXR Distortion+ circuit: gain stage, diode clipping,
  filter topology, and how each maps to the DSP model in `effects/mxr_distortion_plus.cpp`.

- [`docs/circuit-to-patch-conversion.md`](../docs/circuit-to-patch-conversion.md) —
  General methodology for converting analog schematics into SDK patches. Covers
  1-pole IIR filters, waveshapers, gain staging, SDK constraints, and worked examples.
  Intended as a reference for future sessions and contributors.

---

## Testing

Run the automated syntax and lint check from the repository root:

```bash
bash tests/check_patches.sh
```

See [`tests/README.md`](../tests/README.md) for details on what is checked and its limitations.
