# CPU Cycle Budget Reference

This document is the planned home for measured per-primitive and per-patch
cycle costs on the Polyend Endless target (ARM Cortex-M7 @ 720 MHz). It is
intentionally honest about scope: as of this writing, **no measured cycle
data exists in this repo**, so the tables below are scaffolding rather than
a cheat sheet. Populate them as real measurements land.

The `sthompsonjr` fork's `CLAUDE.md` includes a per-primitive cycle table
(`OnePoleFilter ~30–50`, `1D Newton ~400–800`, etc.). Those numbers are not
imported here — they are self-reported and were not validated against this
repo's build, toolchain settings, or test harness. Treat them as orientation
when reading the fork, not as a budget gate for local work.

## Hardware and budget context

From [`endless-reference.md`](endless-reference.md) and the Makefile:

| Item | Value |
|---|---|
| CPU | ARM Cortex-M7 |
| FPU | `fpv5-sp-d16`, hard-float ABI |
| Compile flags | `-O3 -fno-exceptions -fno-rtti -fsingle-precision-constant -Wdouble-promotion -std=c++20` |
| Sample rate | `Patch::kSampleRate = 48000` |
| Target per-sample budget | host firmware-defined; treat ≤15k cycles/sample as the working ceiling |

The 15k cycles/sample number is the figure the fork uses. Polyend has not
published a documented per-sample budget for patch code, so this is a rule
of thumb rather than a contract. If a patch lands close to the ceiling on
real hardware, that is the data we should record here, not the rule of
thumb.

## No DTCM/ITCM placement lever

The Cortex-M7 has tightly-coupled memory (DTCM/ITCM) that is faster and
more deterministic than general RAM, and the `sthompsonjr` fork has begun
annotating hot data for DTCM placement. **That technique does not transfer
to this repo.** This SDK's linker script
([`internal/patch_imx.ld`](../internal/patch_imx.ld)) places the entire
patch image — `.text`, `.rodata`, `.data`, and `.bss` — into one
contiguous 512 KB `RAM` region at `0x80000000`, with no separate DTCM
output section. A patch here is a relocatable image dropped at a fixed
address by the firmware loader; the author cannot choose DTCM placement.
So the available cycle-budget levers are algorithmic (cheaper math, less
per-sample work), not memory-placement. Do not chase DTCM annotations
unless the linker model changes.

The DTCM question is doubly moot here: even if the linker model changed to
expose an explicit TCM output section, DTCM's typical capacity on a
Cortex-M7 (tens to a couple hundred KB, part-dependent) would not have
covered the 9.6 MB working buffer's capacity (`Patch::kWorkingBufferSize`
= 2,400,000 floats) regardless. The math from the section above —
512 KB patch image + 9.6 MB working buffer exceeding any single Cortex-M7
variant's on-chip SRAM — points to the working buffer being backed by
external SDRAM/PSRAM via the M7's FMC controller. See
[`patch-authoring-best-practices.md`'s §6 delay-line-cost bullet](patch-authoring-best-practices.md)
for what that means for delay-heavy patches: an off-chip access is a
materially bigger latency cliff than a same-chip cache/TCM miss would have
been, and a real cycle-count measurement (see below) is the only way to
turn that inference into a number.

## What we measure today

[`scripts/analyze_effects.py`](../scripts/analyze_effects.py) and
[`tests/analyze_effects.sh`](../tests/analyze_effects.sh) measure
**control-law and audio behavior**, not CPU cost. Their probe sweeps
report things like:

- gain-sweep monotonicity and residual span
- unity-point position and clip-ratio at unity
- hold-mode delta vs. base voicing
- limiter/headroom pressure at nominal input

These are the right metrics for the questions the repo currently asks
("does this drive feel pedal-like?"), but they do not give per-primitive
or per-patch cycle counts. We do not currently have a benchmarking harness
that emits cycles/sample numbers.

## How to populate this doc

When a real measurement lands, add a row with:

- the primitive or patch name and a `file:line` reference
- the measurement source (probe binary, hardware run, or external tool)
- the build flags used (so the number is reproducible)
- the sample rate and input scale
- the resulting cycles/sample (or a tight range), and the spread/uncertainty

Two reasonable paths to producing those numbers:

1. **Cycle-counted host runs** of the existing `effect_probe.cpp` harness,
   wrapping `processAudio` in a `__rdtsc`-or-equivalent counter. The
   numbers will be host CPU cycles, not Cortex-M7 cycles, and are useful
   as an *ordering* signal between patches.
2. **Hardware runs on Endless** that record DWT cycle counts around
   `processAudio`. These are the only numbers that should be treated as a
   budget gate. We do not have this wired up today. See
   [`hardware-cycle-measurement-howto.md`](hardware-cycle-measurement-howto.md)
   for the DWT methodology — a walkthrough only, not exercised or
   confirmed against real Endless hardware (no confirmed debug-probe
   access exists), so treat it as a starting point for whoever has a
   probe, not a pre-verified recipe.

Until either path is wired into the validation flow, this doc stays a
scaffold.

## Per-patch cycle costs

| Patch | Build flags | Source | Cycles / sample | Notes |
|---|---|---|---|---|
| _none yet_ | — | — | — | populate as real measurements land |

## Per-primitive cycle costs

| Primitive | `file:line` | Build flags | Source | Cycles / sample | Notes |
|---|---|---|---|---|---|
| _none yet_ | — | — | — | — | populate when isolated probes exist |

## Caveats

- Host-measured numbers are not Cortex-M7 numbers. They are useful for
  comparing patches against each other under the same compiler and host,
  not for asserting a real-hardware budget.
- `-O3` interacts strongly with whether helpers inline. A primitive cost
  measured in isolation is not necessarily the cost it contributes inside
  a real `processAudio` body.
- Do not paste in fork-claimed numbers. If you need a number badly enough
  to estimate, measure it yourself and record the methodology.

## See also

- [`docs/endless-reference.md`](endless-reference.md) — hard rules and
  build flags
- [`tests/analyze_effects.sh`](../tests/analyze_effects.sh) /
  [`scripts/analyze_effects.py`](../scripts/analyze_effects.py) — current
  audio-behavior probe surface
- [`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md)
  — context for what the fork claims and why we did not import it
- [`docs/aliasing-oversampling-experiment.md`](aliasing-oversampling-experiment.md)
  — a worked example of the `__rdtsc` host-cycle-ratio methodology
  described above, measuring the CPU/aliasing tradeoff of 2x-oversampling
  a drive-effect nonlinearity
