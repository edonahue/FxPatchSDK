# Agent Instructions

You are working on a fork of the official
[`polyend/FxPatchSDK`](https://github.com/polyend/FxPatchSDK) for the Polyend
Endless multi-effects pedal. This file is the agent-facing entry point. Read
it first, then follow the reading order below before making changes.

This file deliberately stays short. It points at canonical documents instead
of duplicating them; if a fact lives in two places it will drift.

## Read these first

If you are preparing to add or change code, read in this order:

1. [`docs/repository-review.md`](docs/repository-review.md) — current branch
   and codebase state
2. [`docs/endless-reference.md`](docs/endless-reference.md) — SDK shape,
   hardware constraints, and the canonical "Hard Rules for Patch Authors"
   list (see its §8)
3. [`docs/patch-authoring-best-practices.md`](docs/patch-authoring-best-practices.md)
   — the "how to handcraft a good Endless patch" reference: control-law
   conventions, DSP idioms (with pointers into [`source/dsp/`](source/dsp)),
   working-buffer use patterns, CPU-budget intuition, and the
   handcraft-vs-Playground positioning
4. [`effects/README.md`](effects/README.md) — patch catalog and control
   cheat sheet
5. [`docs/circuit-to-patch-conversion.md`](docs/circuit-to-patch-conversion.md)
   — circuit-to-DSP mapping playbook
6. [`docs/fork-comparisons/`](docs/fork-comparisons) — fork survey:
   [`upstream-and-forks.md`](docs/fork-comparisons/upstream-and-forks.md)
   tracks the upstream repo and the fork network;
   [`sthompsonjr-wdf.md`](docs/fork-comparisons/sthompsonjr-wdf.md) is the
   deep-dive on the `sthompsonjr` fork — what we adopted, what we declined

If you are starting a new patch rather than editing one, also pair
[`docs/templates/patch-build-walkthrough.md`](docs/templates/patch-build-walkthrough.md)
with
[`docs/templates/patch-code-skeleton.cpp`](docs/templates/patch-code-skeleton.cpp).

## Hard constraints

These come from
[`docs/endless-reference.md`](docs/endless-reference.md) §3, §7, §8 and the
Makefile. Do not violate them; do not restate them in patch files.

- ARM Cortex-M7 target, position-independent code.
- Build flags include `-std=c++20`, `-fno-exceptions`, `-fno-rtti`,
  `-fsingle-precision-constant`, `-Wdouble-promotion`, `-O3`.
- No heap: no `new`, `malloc`, `std::vector`, or other dynamic allocation.
- Single-precision floats only; use `f`-suffixed literals.
- Sample rate `Patch::kSampleRate = 48000`. Stereo, equal-length spans,
  in-place buffers.
- Output must stay within `(-1.0f, 1.0f)`; the SDK has no safety net.
- `setParamValue()` runs on the audio thread. Treat it as audio-thread code.
- The working buffer is `2400000` floats, provided once via
  `setWorkingBuffer()`. Use it for large delay/state storage, not
  convenience data.

If you find yourself wanting to relax one of these constraints, stop and ask
before editing. They are enforced by toolchain flags and by discipline; the
review burden is on whoever proposes a change.

## Patch authoring workflow

1. **Pre-flight.** Fill in the table at the top of
   [`docs/templates/patch-build-walkthrough.md`](docs/templates/patch-build-walkthrough.md):
   reference circuit, DSP primitives, working-buffer use, state-variable
   count, knob/expression assignments, LED colors.
2. **Implement.** Start from
   [`docs/templates/patch-code-skeleton.cpp`](docs/templates/patch-code-skeleton.cpp).
   Patch files live in `effects/<patch>.cpp`. The default single-patch
   build target is [`source/PatchImpl.cpp`](source/PatchImpl.cpp); for
   batch builds use [`scripts/build_effects.sh`](scripts/build_effects.sh)
   or [`tests/build_effects.sh`](tests/build_effects.sh) instead of
   manually copying files into `source/PatchImpl.cpp`.
3. **Validate, in order:**
   - [`tests/check_patches.sh`](tests/check_patches.sh) — host-side
     syntax/lint
   - [`tests/build_effects.sh`](tests/build_effects.sh) — real ARM `.endl`
     builds
   - [`tests/analyze_effects.sh`](tests/analyze_effects.sh) — probe sweeps
     for nonlinear growth, unity position, and limiter/headroom behavior
4. **Document.** Author a `docs/<patch>-build-walkthrough.md` (and a
   `docs/<patch>-research.md` if the circuit reference deserves its own
   notes) modeled on existing examples. Capture the *why* of control-law
   choices, not just the *what*.

## Naming and layout

- Patch implementations: `effects/<patch>.cpp` — snake_case, lowercase.
- Per-patch docs: `docs/<patch>-build-walkthrough.md` and optionally
  `docs/<patch>-research.md`.
- Shared DSP primitives: header-only files under
  [`source/dsp/`](source/dsp), in `namespace dsp`. Scope is the proven
  duplicates only — see the "deliberately does not do" list below.
- Per-primitive unit tests: `tests/dsp/<x>_test.cpp`, built and run by
  [`tests/check_dsp.sh`](tests/check_dsp.sh) (which
  [`tests/check_patches.sh`](tests/check_patches.sh) invokes automatically).
- Validation scripts live under `tests/`; tooling under `scripts/`.
- This repo uses snake_case filenames throughout; do not rename toward the
  `sthompsonjr` fork's PascalCase header style.

## CPU budget

Cortex-M7 cycle costs are a real constraint, but the repo has no measured
cycle data today. See [`docs/cycle-budget.md`](docs/cycle-budget.md) for
methodology and the (currently empty) measurement table. Do not import the
fork's self-reported numbers; if you need a budget number, measure it.

Standalone CPU/behavior experiments that inform this repo's decisions but
are not shipped patches live outside `effects/` — e.g.
[`tests/oversample_alias_probe.cpp`](tests/oversample_alias_probe.cpp) /
[`docs/aliasing-oversampling-experiment.md`](docs/aliasing-oversampling-experiment.md),
which measured the alias-reduction/CPU tradeoff of 2x-oversampling a
drive-effect nonlinearity without applying it to any shipped effect. Follow
that pattern for future measure-before-deciding experiments: a probe under
`tests/` (never under `effects/`, so it's never mistaken for a patch), an
analysis script under `scripts/` if needed, and a doc under `docs/`
recording the actual numbers and a data-driven conclusion.

## What this repo deliberately does not do

These have already been considered and declined; the rationale is in
[`docs/fork-comparisons/sthompsonjr-wdf.md`](docs/fork-comparisons/sthompsonjr-wdf.md).
Do not "helpfully" re-introduce them without explicit direction:

- No fork-style inventory triple
  (`build_pipeline.txt`, `library_inventory.txt`, `INVENTORY.md`) — the
  reading order above and `effects/README.md` cover the same surface at
  this scale.
- No `docs_sync`-style auto-regeneration of inventory files.
- No speculative expansion of [`source/dsp/`](source/dsp). The current
  primitives are the proven duplicates from the 2026-06-13 and 2026-06-14
  audits (most recently `dsp::OnePoleLowpass`/`dsp::OnePoleHighpass`,
  extracted when a second effect turned out to already have a
  byte-identical file-local copy). Add a new primitive only when at least
  two effects would use it; otherwise keep it inline in the effect that
  needs it. The scope is "extract the duplicates", not "build a library."
- No SDK API change for per-patch expression routing
  (the fork's `isParamEnabled(...)` hook). Expression is wired to param 2
  via [`internal/PatchCppWrapper.cpp`](internal/PatchCppWrapper.cpp) for
  now; revisit only with explicit user direction.
- No PascalCase rename of `effects/*.cpp` toward fork conventions.
- No bulk WDF migration. Two sibling experiments
  ([`effects/big_muff_wdf.cpp`](effects/big_muff_wdf.cpp),
  [`effects/tube_screamer_wdf.cpp`](effects/tube_screamer_wdf.cpp))
  remain the only WDF-style local patches until a CPU-budget experiment
  on Endless hardware says otherwise. (`effects/dimension_chorus.cpp`,
  added 2026-06-14, is not a third WDF sibling — it doesn't perform a WDF
  nonlinear network solve, so this count is unaffected; see
  [`docs/fork-comparisons/sthompsonjr-wdf.md`](docs/fork-comparisons/sthompsonjr-wdf.md)
  for what it actually is and why. It is also the one deliberate,
  explicitly-approved exception to the general "12 effects, no new ones"
  catalog-freeze decision — see
  [`docs/dimension-chorus-build-walkthrough.md`](docs/dimension-chorus-build-walkthrough.md).
  Do not treat that exception as reopening the freeze generally.)
- No code ports from the fork until its license status is resolved (no
  `LICENSE` file confirmed in the `sthompsonjr` fork as of 2026-05-18).
  Idea-level borrowing via the comparison doc is fine; copying source is
  not.

One fork idea *has* been accepted: a JUCE VST3 wrapper for auditioning
patches on the desktop, from the `andybalham` fork. It is planned but not
built — see [`docs/vst-host-plan.md`](docs/vst-host-plan.md). Upstream
`polyend/FxPatchSDK` is dormant and this fork is fully caught up with it;
[`docs/fork-comparisons/upstream-and-forks.md`](docs/fork-comparisons/upstream-and-forks.md)
is the place to confirm that before spending time on an upstream sync.

## Operational notes for agents

- Branch policy: develop on the branch named in the session's instructions
  (currently `claude/review-fork-docs-3TqDM`). Do not push to `master`.
- Commit small, descriptive changes. Do not amend pushed commits.
- Do not open pull requests unless the user asks.
- This repo is read by both Claude Code and OpenAI Codex; `AGENTS.md`
  redirects Codex here. Keep instructions in one file (this one).
