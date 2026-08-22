# Upstream and Fork Survey

**Last surveyed:** 2026-06-14. The 2026-06-13 walk found the ecosystem static;
the 2026-06-14 follow-up walk found the opposite — `sthompsonjr` resumed
active development, a new fork appeared, one tracked clone was deleted, and a
non-fork reimplementation surfaced. See
[Changes since the 2026-06-13 walk](#changes-since-the-2026-06-13-walk) below.
Upstream itself remains dormant throughout.

A note on dates: live GitHub data returned by this walk's tooling reflected
commit timestamps past this session's own stated "today." Rather than
reconcile or hide that, this doc records what GitHub actually reports,
timestamped precisely, and flags the discrepancy here once rather than
re-explaining it at every finding below.

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

Upstream has six forks as of 2026-06-14 — the same count as before
2026-06-13's `klausmobi32` addition, but the composition changed:
`Everplay-Tech/FxPatchSDK` was deleted, `klausmobi32/FxPatchSDK` is no longer
fork-linked (still exists as a bare, empty, unlinked repo — see the changes
section below), and a new fork `sandroidmusic/FxPatchSDK` appeared. Two forks
carry meaningful independent work.

| Fork | Own commits | Verdict |
| --- | --- | --- |
| [`sthompsonjr/Endless-FxPatchSDK`](https://github.com/sthompsonjr/Endless-FxPatchSDK) | 89 own (94 total, on 5 upstream) | Substantial and **active again** as of 2026-06-14 — resumed after a ~5-week gap. New DC-2/TriDimension stereo-chorus circuit family. See [`sthompsonjr-wdf.md`](sthompsonjr-wdf.md). |
| [`andybalham/FxPatchSDK`](https://github.com/andybalham/FxPatchSDK) | 2 (atop the 5 upstream) | Small but has one genuinely useful idea — see below. Unchanged since 2026-03-22. |
| `edonahue/FxPatchSDK` | — | This repo. |
| [`sandroidmusic/FxPatchSDK`](https://github.com/sandroidmusic/FxPatchSDK) | 1 (atop the 5 upstream) | New fork, created 2026-07-05. Adds WASM/`.endless` compilation support. Small — one commit — but a genuinely distinct idea (a build-pipeline change, not a DSP/effect contribution) not overlapping with `sthompsonjr` or `andybalham`. Not yet substantial enough for a dedicated deep-dive; worth a look at what the WASM path actually changes if it grows. |
| `johnnyclem/FxPatchSDK` | 0 | Plain clone, no custom work. |
| `scarlton/FxPatchSDK` | 0 | Plain clone (only `02f22ca`), no custom work. |

The two "no own work" clones (`johnnyclem`, `scarlton`) are listed only so a
future walk does not re-investigate them from scratch.

A separate non-SDK project deserves a one-line mention so a search for
"FxPatchSDK" or "endless" does not surface it as a fork: `Everplay-Tech/endless-surfer`
was a TypeScript ecosystem/forge project, not a C++ patch fork — see the
changes section below for its (and its parent repo's) removal.

### Non-fork implementations

Not everyone referencing this SDK does so by forking it. Worth tracking
alongside the fork network:

- [`ceejbot/endless-rs`](https://github.com/ceejbot/endless-rs) — a Rust
  reimplementation of the SDK (MIT-licensed), targeting
  `thumbv7em-none-eabihf` with `#![no_std]`. It exposes a safe `Patch` trait
  plus a macro generating the ABI glue to produce `.endl` binaries, and
  includes a full Rust port of the official bitcrush example. Not a code
  source for this C++ repo (different language), but its documented hard
  constraints — no heap allocation, single-precision floats only, output in
  `(-1.0, 1.0)`, real-time deadlines must never be missed — are useful as
  **independent corroboration** of the same rules this repo already
  documents in [`endless-reference.md`](../endless-reference.md) §7/§8. Two
  people arriving at the same constraint list from different directions (a
  C++ reverse-engineering of the firmware ABI here, a from-scratch Rust
  binding there) is a reasonable signal those constraints are real, not
  over-cautious house rules.
- The Polyend GitHub org (<https://github.com/polyend>) now hosts three
  repos, not one: `FxPatchSDK` (unchanged, see above), `PresetSandbox` (a
  much older, unrelated sandbox for the Preset device, last touched
  2020-12-22 — pre-existing but not previously logged in this survey), and
  `tracker-lib` (active, 15 commits, last updated 2026-06-25 — a TypeScript
  library for reading/writing Polyend **Tracker** project files, i.e. scoped
  to the Tracker product line, not Endless; no FxPatchSDK relation found in
  its README). Neither is Endless-SDK-relevant; noted here so a future walk
  doesn't need to re-check the org's repo list from scratch.

## Changes since the 2026-06-13 walk

This section is a delta only — the material above already reflects these
changes; this section is where the "what moved and when" record lives.

- **`sthompsonjr/Endless-FxPatchSDK` resumed development.** Total commit
  count 79→94 (own commits ~74→89) — see
  [`sthompsonjr-wdf.md`](sthompsonjr-wdf.md) for the full delta (new
  DC-2/TriDimension circuit family, `dsp/`/`wdf/` file counts, LICENSE
  status unchanged).
- **`sandroidmusic/FxPatchSDK` appeared** — new fork, created 2026-07-05, 1
  own commit ("feat: add wasm / .endless compilation support").
- **`Everplay-Tech/FxPatchSDK` was deleted.** The repo now 404s; the
  `Everplay-Tech` account's repo list shows only one unrelated project
  (`aimanac-cli`, a Python CLI, last updated 2026-08-08). Its companion
  project `Everplay-Tech/endless-surfer` (the TypeScript ecosystem/forge
  project mentioned above) was **also deleted** — Everplay-Tech appears to
  have pivoted away from Endless-related work entirely.
- **`klausmobi32/FxPatchSDK` is no longer fork-linked.** The repo still
  exists and is still empty, but upstream's `/network/members` fork list no
  longer includes it, and a direct check reports it is "not marked as a
  fork" of `polyend/FxPatchSDK`. Whether it was explicitly un-forked,
  deleted-and-recreated independently, or this is a stale listing artifact
  is not determinable from the outside. Net effect on the fork count: this
  repo's removal from the network offsets `sandroidmusic`'s addition and
  `Everplay-Tech`'s deletion, landing back at six forks total (see table
  above).
- Upstream, `andybalham`, `johnnyclem`, and `scarlton` were re-checked and
  are confirmed unchanged.

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
