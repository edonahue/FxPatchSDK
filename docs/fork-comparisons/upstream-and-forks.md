# Upstream and Fork Survey

**Last surveyed:** 2026-06-13. The 2026-06-13 walk found the ecosystem static
since 2026-05-18 — no fork in the network has committed in the intervening
~four weeks, and upstream has not committed since 2026-03-17. The only net-new
finding is one additional empty fork (`klausmobi32/FxPatchSDK`).

This note tracks two things the per-fork deep-dives do not:

1. the upstream `polyend/FxPatchSDK` repo — whether this fork is behind it; and
2. the rest of the fork network — every fork of upstream other than the one with
   its own deep-dive.

The `sthompsonjr` fork has a dedicated deep-dive in
[`sthompsonjr-wdf.md`](sthompsonjr-wdf.md); this note does not repeat it.

## Wider context

The hardware itself shipped in February 2026 and won Best in Show at NAMM 2026,
so commercial interest is real even if the open-source SDK has not picked up
activity to match. The active community lives on
<https://backstage.polyend.com> — Polyend's own forum, where threads on
Playground prompts, feature requests (notably momentary footswitch / tap-tempo
mode, more LED colors, a desktop plugin), and pedalboard sharing run. Future
walks should glance at Backstage too — GitHub is no longer the centre of
gravity for Endless patch authoring.

## Upstream: `polyend/FxPatchSDK`

<https://github.com/polyend/FxPatchSDK> — the official SDK this repo is forked from.

**Status as of 2026-06-13: dormant, and this fork is fully caught up.** Reconfirmed
at this walk — still no new commits since 2026-03-17, still no tags or releases,
GitHub Discussions still disabled.

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
  examples would be a low-effort, high-goodwill move. See the closed PR #3 note
  below for why we are not pursuing this for now.

**Closed PR #3 — our own previous attempt.** PR #3 ("Claude/continue effects
patches e fz ux") was opened on this fork's behalf on 2026-04-16 and proposed
21 commits — SDK documentation, circuit analysis for the MXR Distortion+ and
DOD 250, and seven effect patches (chorus, distortions, wah, phaser, enhancer,
Big Muff, reverse delay) — substantially the body of work this repo now
contains. It was closed without merge on 2026-05-01 (the same day issue #4 was
opened asking for the same thing, ironically). The 2026-06-13 design review
explicitly decided **not to re-engage upstream**: this fork's mission is to be
a craft-and-best-practices reference for handcrafting Endless patches as a
counterpoint to the Polyend Playground AI-generation flow, and that mission is
served better by deepening the fork than by upstream lobbying. The PR remains
public on GitHub; the work itself lives on in this repo.

**Next walk:** re-check the commit list. If upstream moves, the first question is
whether it touched `source/Patch.h`, `internal/PatchABI.h`, or
`internal/PatchCppWrapper.cpp` — those are the ABI/API surface a patch depends on.

## Fork network

Upstream has seven forks as of 2026-06-13 (six at the previous walk plus one new
empty one). Only two carry meaningful independent work, and neither has moved in
~four weeks.

| Fork | Own commits | Verdict |
| --- | --- | --- |
| [`sthompsonjr/Endless-FxPatchSDK`](https://github.com/sthompsonjr/Endless-FxPatchSDK) | ~74 own (79 total, on 5 upstream) | Substantial but stalled — last commit 2026-05-06. WDF/DSP library + DMM. See [`sthompsonjr-wdf.md`](sthompsonjr-wdf.md). |
| [`andybalham/FxPatchSDK`](https://github.com/andybalham/FxPatchSDK) | 2 (atop the 5 upstream) | Small but has one genuinely useful idea — see below. Unchanged since 2026-03-22. |
| `edonahue/FxPatchSDK` | — | This repo. |
| `Everplay-Tech/FxPatchSDK` | 0 | Plain clone, no custom work. |
| `johnnyclem/FxPatchSDK` | 0 | Plain clone, no custom work. |
| `scarlton/FxPatchSDK` | 0 | Plain clone (only `02f22ca`), no custom work. |
| `klausmobi32/FxPatchSDK` | 0 | New since the last walk — empty fork, no content. Listed here so a future walk does not re-investigate. |

The four "no own work" clones (`Everplay-Tech`, `johnnyclem`, `scarlton`,
`klausmobi32`) are listed only so a future walk does not re-investigate them
from scratch.

A separate non-SDK project deserves a one-line mention so a search for
"FxPatchSDK" or "endless" does not surface it as a fork: `Everplay-Tech/endless-surfer`
is a TypeScript ecosystem/forge project, not a C++ patch fork, and is orthogonal
to anything tracked here.

### `andybalham/FxPatchSDK`

Two own commits (2026-03-22): "Added Claude.md and effect examples" and "Added VST3
support." MIT-licensed (LICENSE.TXT present, copyright Polyend 2026). It is a
small, pedagogical fork — but the VST3 work is the single most useful idea found
anywhere in the fork network.

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
