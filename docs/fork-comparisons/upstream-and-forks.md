# Upstream and Fork Survey

**Last surveyed:** 2026-05-18.

This note tracks two things the per-fork deep-dives do not:

1. the upstream `polyend/FxPatchSDK` repo — whether this fork is behind it; and
2. the rest of the fork network — every fork of upstream other than the one with
   its own deep-dive.

The `sthompsonjr` fork has a dedicated deep-dive in
[`sthompsonjr-wdf.md`](sthompsonjr-wdf.md); this note does not repeat it.

## Upstream: `polyend/FxPatchSDK`

<https://github.com/polyend/FxPatchSDK> — the official SDK this repo is forked from.

**Status as of 2026-05-18: dormant, and this fork is fully caught up.**

Upstream has exactly five commits, all from the initial release window:

| Commit | Message | Date |
| --- | --- | --- |
| `02f22ca` | Initial Beta version of the SDK | 2026-01-23 |
| `7df0cac` | Corrects agent buffer size calculation | 2026-03-17 |
| `284a60f` | Adds MIT License and updates README | 2026-03-17 |
| `1a93632` | Configures patch output name and extension | 2026-03-17 |
| `708f08d` | Refactors parameter enablement logic | 2026-03-17 |

`git log` on this repo's `master` shows all five as the root of our history, so this
fork's base contains 100% of upstream. There is no upstream change to merge. Upstream
has not committed since 2026-03-17 and publishes no tagged releases.

What the last upstream commit (`708f08d`) actually did is worth recording, because the
`sthompsonjr` deep-dive discusses an `isParamEnabled` divergence: upstream's
parameter-enablement model is the C ABI function
`patch_agent_is_param_enabled(env, idx, sourceId)` in
[`internal/PatchCppWrapper.cpp`](../../internal/PatchCppWrapper.cpp), where `sourceId`
distinguishes knob (`0`) from expression (`1`). The ABI already carries the per-source
concept; our wrapper just hard-codes the routing (expression → param 2). The
`sthompsonjr` fork's divergence is that it lifts that decision into a pure virtual on
the C++ `Patch` class — a C++ API change, not an ABI change. The shared C ABI is the
same in both repos.

**Open upstream issues worth knowing:**

- Issue #1 (opened 2026-02-20) — request to expose persistent storage. Confirms there
  is **no persistent-storage / preset-save facility** in the SDK. This is recorded as a
  known limitation in [`docs/endless-reference.md`](../endless-reference.md).
- Issue #4 (opened 2026-05-01) — request for "more complex code samples." This fork
  already has twelve documented effects; contributing one or more upstream as worked
  examples is a low-effort, high-goodwill opportunity if we ever want to engage
  upstream. Not a current task — just noted.

**Next walk:** re-check the commit list. If upstream moves, the first question is
whether it touched `source/Patch.h`, `internal/PatchABI.h`, or
`internal/PatchCppWrapper.cpp` — those are the ABI/API surface a patch depends on.

## Fork network

Upstream has six forks. Only two carry meaningful independent work.

| Fork | Own commits | Verdict |
| --- | --- | --- |
| [`sthompsonjr/Endless-FxPatchSDK`](https://github.com/sthompsonjr/Endless-FxPatchSDK) | ~74 | Substantial. WDF/DSP library + DMM. See [`sthompsonjr-wdf.md`](sthompsonjr-wdf.md). |
| [`andybalham/FxPatchSDK`](https://github.com/andybalham/FxPatchSDK) | 2 (atop the 5 upstream) | Small but has one genuinely useful idea — see below. |
| `edonahue/FxPatchSDK` | — | This repo. |
| `Everplay-Tech/FxPatchSDK` | 0 | Plain clone, no custom work. |
| `johnnyclem/FxPatchSDK` | 0 | Plain clone, no custom work. |
| `scarlton/FxPatchSDK` | 0 | Plain clone, no custom work. |

The three plain clones were checked on 2026-05-18 and carry nothing beyond the five
upstream commits. They are listed here only so a future walk does not re-investigate
them from scratch.

### `andybalham/FxPatchSDK`

Two own commits (2026-03-22): "Added Claude.md and effect examples" and "Added VST3
support." It is a small, pedagogical fork — but the VST3 work is the single most
useful idea found anywhere in the fork network.

- **`vst/` — a JUCE VST3 wrapper.** A CMake project that pulls JUCE via FetchContent
  and wraps an effect in a `juce::AudioProcessor`, so the *same* patch DSP can be
  auditioned in a DAW (or a standalone app) before it is ever flashed to hardware.
  Our repo can syntax-check, ARM-build, and run a synthetic probe, but it has no way
  to *listen* to a patch without the pedal. A desktop wrapper closes that gap.
  This idea has been adopted and implemented in [`vst/`](../../vst); the design
  rationale is [`docs/vst-host-plan.md`](../vst-host-plan.md).
- **`source/effects/` — six classic effects** (Bitcrush, Saturation, Distortion,
  Delay, Flanger, Reverb) written as header-only `Patch` subclasses. They are
  textbook implementations (Freeverb, LFO-modulated delay, etc.) and not more
  advanced than this repo's twelve effects; no porting need.
- **Its own `CLAUDE.md`** — a teaching-oriented SDK guide. Our `CLAUDE.md` is
  deliberately a terse pointer file instead; no change wanted.

Nothing else in `andybalham` needs adopting. The VST3 idea is the takeaway.

## Where this fork stands

For perspective: among the six forks, this one (`edonahue`) is the most
documentation- and tooling-complete — twelve documented effects, validation scripts,
circuit-to-DSP playbooks, and per-patch walkthroughs. `sthompsonjr` has the broader
reusable WDF/DSP *library*; this fork has the deeper authoring *process*. The two
forks are strong in different dimensions, which is why the comparison docs treat
`sthompsonjr` as an idea source rather than a merge target.

## References

- [`sthompsonjr-wdf.md`](sthompsonjr-wdf.md) — deep-dive on the `sthompsonjr` fork
- [`docs/vst-host-plan.md`](../vst-host-plan.md) — plan to adopt the VST3 audition idea
- [`docs/endless-reference.md`](../endless-reference.md) — SDK reference, incl. known limitations
- <https://github.com/polyend/FxPatchSDK> — upstream
- <https://github.com/polyend/FxPatchSDK/forks> — fork network
- <https://github.com/andybalham/FxPatchSDK> — the VST3-wrapper fork
