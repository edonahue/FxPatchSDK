# effects/

This directory is where real patch development happens in this fork. Everything here is
intended to stay compatible with the stock `polyend/FxPatchSDK` layout unless a file is
explicitly marked otherwise.

Current inventory:

- `back_talk_reverse_delay.cpp`: Back Talk-inspired reverse delay with a hold-toggle texture mode
- `bbe_sonic_stomp.cpp`: guitar-oriented sonic enhancer inspired by BBE Sonic Stomp / Aion Lumin
- `big_muff.cpp`: Ram's Head-inspired Big Muff fuzz with a Tone Bypass alternate voice and expression-as-blend
- `chorus.cpp`: stereo chorus with modulated delay lines
- `klon_centaur.cpp`: Klon-inspired transparent overdrive with a Tone Mod alternate voice and expression-as-output
- `mxr_distortion_plus.cpp`: MXR Distortion+ inspired distortion with Endless-tuned gain, tone, and level control
- `phase_90.cpp`: one-knob Phase 90 phaser with a block/script voice toggle
- `tube_screamer.cpp`: TS808-inspired overdrive with a TS9 alternate voice and expression-as-tone
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

## Control-Law Review

Patch review in this fork now treats parameter taper as a first-class design choice.

- `mxr_distortion_plus.cpp` is the main cautionary case: literal analog pot math made the
  hardware controls feel bunched and uneven, so the patch now uses Endless-tuned curves
- `big_muff.cpp` is the main tone-stack/blend case: classic Muff controls need output
  compensation and a non-literal third control if expression is meant to stay musical
- `klon_centaur.cpp` is the main clean/dirty summing case: the gain control must rebalance
  internal paths instead of behaving like a single-path drive knob
- `phase_90.cpp` is the main minimal-UI case: one public knob, intentional unused controls,
  and a mode switch that changes voicing without adding more UI surface
- `tube_screamer.cpp` is the main expression-on-tone case: the classic control layout is
  worth preserving, but the tone sweep must stay musical on both the knob and the pedal
- `chorus.cpp` is a good positive example: log taper for `Rate`, linear mappings for `Depth` and `Mix`
- `wah.cpp` is another positive example: log taper for sweep frequency, linear mapping for Q
- `bbe_sonic_stomp.cpp` uses bounded blend/offset mappings rather than trying to mimic analog-pot laws
- `back_talk_reverse_delay.cpp` uses a log-style time mapping, bounded feedback, and an equal-power mix to keep expression musical

When adding or reviewing a patch, verify four things for every knob:

- the control does useful audible work across most of its travel
- the taper matches the job
- the default lands on a usable sound
- param `2` still makes sense when expression takes over the Right knob

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

- [`docs/back-talk-reverse-delay-build-walkthrough.md`](../docs/back-talk-reverse-delay-build-walkthrough.md) —
  design log for `back_talk_reverse_delay.cpp`
- [`docs/bbe-sonic-stomp-build-walkthrough.md`](../docs/bbe-sonic-stomp-build-walkthrough.md) —
  design log for `bbe_sonic_stomp.cpp`
- [`docs/bbe-sonic-stomp-research.md`](../docs/bbe-sonic-stomp-research.md) —
  public circuit and clone research summary for the Sonic Stomp / Lumin family
- [`docs/big-muff-research.md`](../docs/big-muff-research.md) —
  Electrosmash-grounded variant and control-surface rationale for `big_muff.cpp`
- [`docs/big-muff-build-walkthrough.md`](../docs/big-muff-build-walkthrough.md) —
  design log for `big_muff.cpp`
- [`docs/klon-centaur-research.md`](../docs/klon-centaur-research.md) —
  ElectroSmash-grounded clean/dirty summing and mod rationale for `klon_centaur.cpp`
- [`docs/klon-centaur-build-walkthrough.md`](../docs/klon-centaur-build-walkthrough.md) —
  design log for `klon_centaur.cpp`
- [`docs/phase-90-research.md`](../docs/phase-90-research.md) —
  ElectroSmash-grounded one-knob phaser and script-mod rationale for `phase_90.cpp`
- [`docs/phase-90-build-walkthrough.md`](../docs/phase-90-build-walkthrough.md) —
  design log for `phase_90.cpp`
- [`docs/tube-screamer-research.md`](../docs/tube-screamer-research.md) —
  ElectroSmash-grounded family and control-surface rationale for `tube_screamer.cpp`
- [`docs/tube-screamer-build-walkthrough.md`](../docs/tube-screamer-build-walkthrough.md) —
  design log for `tube_screamer.cpp`
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
