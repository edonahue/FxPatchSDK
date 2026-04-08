# effects/

This directory is where real patch development happens in this fork. Everything here is
intended to stay compatible with the stock `polyend/FxPatchSDK` layout unless a file is
explicitly marked otherwise.

Current inventory:

- `bbe_sonic_stomp.cpp`: guitar-oriented sonic enhancer inspired by BBE Sonic Stomp / Aion Lumin
- `chorus.cpp`: stereo chorus with modulated delay lines
- `mxr_distortion_plus.cpp`: circuit-informed distortion patch
- `wah.cpp`: dual-mode wah with expression-driven sweep
- `examples/reverb.cpp`: imported reference example that still compiles against the stock SDK

---

## Recommended Workflow

Each top-level `effects/*.cpp` file is a self-contained `Patch` implementation.

To build one with the current repo layout:

1. Copy the patch file into `source/PatchImpl.cpp`
2. Change `#include "../source/Patch.h"` to `#include "Patch.h"`
3. Build:
   ```bash
   make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=my_effect
   ```
4. Deploy the resulting `.endl` file onto the Endless USB volume

Or use the VSCode tasks in `.vscode/tasks.json`.

Before hardware testing:

```bash
bash tests/check_patches.sh
```

---

## Patch Authoring Notes

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

Rules that matter in practice:

- No heap (`malloc`, `new`, `std::vector`) — all state must be members or in the working buffer
- Output must stay in `(-1.0f, 1.0f)` — no automatic limiting
- No exceptions, no RTTI
- Write `0.5f` not `0.5` (single-precision only; `-Wdouble-promotion` will catch you)

---

## Expression pedal

Current repo behavior: expression pedal is hardcoded to param `2` for every patch.
This is implemented in `internal/PatchCppWrapper.cpp`. Heel down = `0.0f`, toe down
= `1.0f`, same range as the Right knob. When the pedal is connected, the firmware
uses pedal values for param `2` and ignores the physical Right knob.

Implications for patch design:

- If your Right knob is a live-performance control (mix, level, depth) — expression
  pedal works automatically with no extra code.
- If your Right knob is a set-and-forget parameter (circuit selector, mode switch,
  etc.) — either swap which knob is your live-performance target, or change the
  `idx == 2` condition in `PatchCppWrapper.cpp` to match.

Future improvement: add a virtual `isParamEnabled()` method to `Patch.h` so each patch
declares its own expression routing without editing shared infrastructure.

See [`docs/endless-reference.md`](../docs/endless-reference.md) for the full SDK reference.

---

## Design Documents

These are the best files to read before editing or adding a patch:

- [`docs/bbe-sonic-stomp-build-walkthrough.md`](../docs/bbe-sonic-stomp-build-walkthrough.md) —
  design log for `bbe_sonic_stomp.cpp`
- [`docs/bbe-sonic-stomp-research.md`](../docs/bbe-sonic-stomp-research.md) —
  public circuit and clone research summary for the Sonic Stomp / Lumin family
- [`docs/wah-build-walkthrough.md`](../docs/wah-build-walkthrough.md) —
  complete design log for `wah.cpp`
- [`docs/mxr-distortion-plus-circuit-analysis.md`](../docs/mxr-distortion-plus-circuit-analysis.md) —
  circuit-to-DSP analysis for `mxr_distortion_plus.cpp`
- [`docs/circuit-to-patch-conversion.md`](../docs/circuit-to-patch-conversion.md) —
  general methodology for mapping analog pedal ideas into SDK-safe DSP
- [`docs/templates/patch-build-walkthrough.md`](../docs/templates/patch-build-walkthrough.md) —
  starting point for documenting a new patch

---

## Testing

```bash
bash tests/check_patches.sh
```

This is a host-side preflight, not a replacement for a real ARM build or on-device
listening test. See [`tests/README.md`](../tests/README.md).
