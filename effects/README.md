# effects/

This directory is for custom patches built on top of the FxPatchSDK.

---

## Workflow

Each patch is a self-contained C++ implementation of the `Patch` abstract class.
To build and deploy one:

1. **Copy** the patch file to `source/PatchImpl.cpp` (replacing the existing file)
2. **Build:**
   ```bash
   make TOOLCHAIN=/usr/bin/arm-none-eabi- PATCH_NAME=my_effect
   ```
3. **Deploy:** connect Endless via USB-C, copy `build/my_effect_<timestamp>.endl`
   to the Endless drive

Or use the VSCode **Build** / **Build + Deploy** tasks (`.vscode/tasks.json`).

---

## Structure

```
effects/
  README.md         ← you are here
  examples/         ← reference implementations from community forks
    README.md
    reverb.cpp      ← Freeverb stereo reverb (compiles against stock SDK)
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

See [`docs/endless-reference.md`](../docs/endless-reference.md) for the full SDK reference.
