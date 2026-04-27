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
3. [`effects/README.md`](effects/README.md) — patch catalog and control
   cheat sheet
4. [`docs/circuit-to-patch-conversion.md`](docs/circuit-to-patch-conversion.md)
   — circuit-to-DSP mapping playbook
5. [`docs/fork-comparisons/sthompsonjr-wdf.md`](docs/fork-comparisons/sthompsonjr-wdf.md)
   — what the `sthompsonjr` fork does, what we adopted, what we declined

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
- Validation scripts live under `tests/`; tooling under `scripts/`.
- This repo uses snake_case filenames throughout; do not rename toward the
  `sthompsonjr` fork's PascalCase header style.

## CPU budget

Cortex-M7 cycle costs are a real constraint, but the repo has no measured
cycle data today. See [`docs/cycle-budget.md`](docs/cycle-budget.md) for
methodology and the (currently empty) measurement table. Do not import the
fork's self-reported numbers; if you need a budget number, measure it.

## What this repo deliberately does not do

These have already been considered and declined; the rationale is in
[`docs/fork-comparisons/sthompsonjr-wdf.md`](docs/fork-comparisons/sthompsonjr-wdf.md).
Do not "helpfully" re-introduce them without explicit direction:

- No fork-style inventory triple
  (`build_pipeline.txt`, `library_inventory.txt`, `INVENTORY.md`) — the
  reading order above and `effects/README.md` cover the same surface at
  this scale.
- No `docs_sync`-style auto-regeneration of inventory files.
- No SDK API change for per-patch expression routing
  (the fork's `isParamEnabled(...)` hook). Expression is wired to param 2
  via [`internal/PatchCppWrapper.cpp`](internal/PatchCppWrapper.cpp) for
  now; revisit only with explicit user direction.
- No PascalCase rename of `effects/*.cpp` toward fork conventions.
- No bulk WDF migration. Two sibling experiments
  ([`effects/big_muff_wdf.cpp`](effects/big_muff_wdf.cpp),
  [`effects/tube_screamer_wdf.cpp`](effects/tube_screamer_wdf.cpp))
  remain the only WDF-style local patches until a CPU-budget experiment
  on Endless hardware says otherwise.
- No code ports from the fork until its license status is resolved (no
  `LICENSE` file confirmed there as of 2026-04-27). Idea-level borrowing
  via the comparison doc is fine; copying source is not.

## Operational notes for agents

- Branch policy: develop on the branch named in the session's instructions
  (currently `claude/review-fork-docs-3TqDM`). Do not push to `master`.
- Commit small, descriptive changes. Do not amend pushed commits.
- Do not open pull requests unless the user asks.
- This repo is read by both Claude Code and OpenAI Codex; `AGENTS.md`
  redirects Codex here. Keep instructions in one file (this one).
