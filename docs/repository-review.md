# Repository Review: Fork State, Branches, and Development Direction

Reviewed on 2026-04-07 against local `master`, remote
`origin/claude/polyend-endless-docs-OJnCb`, the official Polyend product pages, and the
official upstream SDK repository.

---

## Summary

This fork has moved well beyond the official `polyend/FxPatchSDK` without abandoning its
core architecture. It keeps the stock SDK API and ABI, then adds three substantial custom
patches, host-side validation, reverse-engineered documentation, and a growing archive of
Playground outputs for reference.

The key branch conclusion is straightforward:

- `master` is the current development baseline.
- `origin/claude/polyend-endless-docs-OJnCb` is older and behind `master`.
- There is no evidence that `claude` contains newer code that should supersede `master`.

Future work should branch from `master`.

---

## Branch Findings

### `master`

`master` contains:

- the older claude-era documentation work
- stereo chorus
- expression-pedal notes and current hardcoded routing
- MXR Distortion+ patch and analysis
- dual-mode wah patch and walkthrough
- syntax/lint validation in `tests/`
- Playground example directories and SpiralCaster artifacts

This is the branch with the broadest practical development value.

### `origin/claude/polyend-endless-docs-OJnCb`

This branch appears to be a historical documentation branch. Relative to `master`, it lacks:

- MXR Distortion+ work
- wah work
- the test directory
- the Playground example archive

Its meaningful documentation additions are already present or superseded on `master`.

### Merge conclusion

Do not merge `claude` into `master` directly. The sensible move is to keep `master` as the
source of truth and preserve `claude` only as history.

---

## Codebase Understanding

### Upstream-compatible core

The architectural center of gravity is still the official SDK layout:

- `source/Patch.h`: developer-facing API
- `internal/PatchABI.h`: firmware-facing ABI
- `internal/PatchCppWrapper.cpp`: bridge between the two
- `Makefile`: `.endl` image generation

That matters because it keeps this fork close enough to upstream that future sync work is
still tractable.

### Where the fork adds real value

- `effects/`: actual custom DSP work, not just the stock bitcrusher example
- `docs/`: reasoning and design records that make future edits safer
- `tests/`: lightweight guardrails before hardware deployment
- `playground/`: artifact collection showing what the hosted Polyend workflow produces

### Present custom-patch profile

- `chorus.cpp`: practical delay-line and LFO example
- `mxr_distortion_plus.cpp`: good first circuit-to-DSP distortion reference
- `wah.cpp`: strongest example of stateful control behavior and mode design in the repo

The codebase now supports more than “SDK orientation”; it supports real patch iteration.

---

## What the Repo Teaches Well Today

- How the stock Endless SDK is structured
- How to write patches that remain compatible with the stock SDK
- How to map analog pedal concepts into simple DSP blocks
- How the current fork routes expression control in practice
- How to validate custom patch files before hardware testing

The documentation now gives enough context for another engineer to start feature work
without having to rediscover the repo shape from scratch.

---

## Current Gaps

### 1. Expression routing remains global

The largest architectural rough edge is the hardcoded expression-pedal routing in
`internal/PatchCppWrapper.cpp`. It is manageable for current patches, but it will become a
real design constraint as soon as the fork adds more multi-mode or circuit-selection effects.

### 2. Build workflow is still copy-based

The repo still expects developers to copy a selected effect into `source/PatchImpl.cpp`
before building. That is acceptable for a small fork, but it is a friction point for
larger effect inventories.

### 3. No real hardware or ARM smoke test in the repo

`tests/check_patches.sh` is useful but intentionally shallow. There is no automated check
that the cross-toolchain is installed, that `make` succeeds for a chosen patch, or that a
generated `.endl` artifact is structurally sane on this machine.

### 4. Playground assets are not yet tied into a repeatable review workflow

The repository contains useful `.endl` and PDF examples, but there is no indexed summary
of what sonic categories they cover, how they compare to the hand-written effects, or which
ones are most valuable as future inspiration or listening references.

---

## Recommended Next Development Moves

### High value

1. Add per-patch expression routing to `Patch.h` and `PatchCppWrapper.cpp` with a safe
default that preserves current behavior.
2. Replace the copy-into-`source/PatchImpl.cpp` build flow with a parameterized effect
selection mechanism in the Makefile.
3. Add one real ARM build smoke test path for at least one effect.

### Medium value

1. Expand the effect library with one time-domain patch and one dynamics patch.
2. Add tiny host-side golden tests for deterministic helper code where practical.
3. Index the Playground archive by effect family and likely DSP category.

### Lower value but still useful

1. Add a concise changelog or release-notes style history for the fork.
2. Add a compatibility note when upstream SDK changes land.

---

## Official Platform Understanding

Polyend’s official materials present Endless as a programmable pedal plus a hosted creation
platform. The distinction matters for this fork:

- Playground is excellent for rapid effect generation and idea exploration.
- The SDK path is the right fit when you need source control, maintainability, deliberate
  DSP choices, and repeatable iteration.

That split is now reflected directly in this repository:

- `effects/` represents the SDK path
- `playground/` represents observation of the hosted path

---

## Validation Snapshot

At review time, `bash tests/check_patches.sh` passes cleanly for:

- `effects/chorus.cpp`
- `effects/mxr_distortion_plus.cpp`
- `effects/wah.cpp`

That does not prove hardware correctness, but it does confirm that the current custom patch
set is syntactically healthy and respects the repo’s host-side lint expectations.

---

## Practical Takeaway

Treat this fork as a stock-SDK-compatible Endless development lab. The most important
conceptual anchors for future work are:

- stay close to upstream in core SDK files
- do real development in `effects/`
- use `docs/` to preserve design intent
- treat Playground artifacts as reference material, not editable source
- branch from `master`
