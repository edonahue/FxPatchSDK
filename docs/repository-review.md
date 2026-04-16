# Repository Review: Fork State, Branches, and Development Direction

Reviewed on 2026-04-16 against local `master`, remote
`origin/claude/polyend-endless-docs-OJnCb`, the official Polyend product pages, and the
official upstream SDK repository.

---

## Summary

This fork has moved well beyond the official `polyend/FxPatchSDK` without abandoning its
core architecture. It keeps the stock SDK API and ABI, then adds nine custom patches
spanning five effect families, a full batch build pipeline, host-side validation, an
effects-deep-dive audit with hardware-verified drive retunes, and a growing archive of
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
- nine custom patches: `chorus`, `mxr_distortion_plus`, `wah`, `big_muff`, `tube_screamer`,
  `klon_centaur`, `phase_90`, `bbe_sonic_stomp`, and `back_talk_reverse_delay`
- expression-pedal notes and current hardcoded routing
- batch build pipeline: `scripts/build_effects.sh` + `tests/build_effects.sh` with PTCH header verification
- host-side syntax/lint validation in `tests/check_patches.sh`
- host-side audio analysis tooling in `tests/analyze_effects.sh`
- `docs/effects-deep-dive-audit.md` with before/after sine-residual drive retune data
- per-patch research and build-walkthrough docs for every custom effect
- Playground example directories, SpiralCaster artifacts, and official Polyend Plates archive

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

**Drive and fuzz:**
- `tube_screamer.cpp`: TS808/TS9 overdrive family; main case study for expression-on-tone and variant voicing
- `klon_centaur.cpp`: transparent overdrive with clean/dirty summing; main case study for gain-dependent path rebalancing
- `mxr_distortion_plus.cpp`: Distortion+ with Endless-tuned control law; main case study for control-law design over analog-literal math
- `big_muff.cpp`: Ram's Head fuzz with Tone Bypass alternate; main case study for passive tone-stack adaptation and expression-as-blend

**Modulation and time:**
- `chorus.cpp`: stereo modulated-delay chorus; practical delay-line and LFO example
- `phase_90.cpp`: one-knob Phase 90 phaser; main case study for intentional minimal-UI and mode-driven voicing
- `wah.cpp`: dual-mode Crybaby/Vox wah; strongest example of stateful control behavior and SVF mode design
- `back_talk_reverse_delay.cpp`: reverse delay with feedback and hold-toggle texture mode; main case study for chunk-buffer state management and equal-power expression-as-mix

**Enhancement:**
- `bbe_sonic_stomp.cpp`: 3-band correction enhancer with optional stereo doubler; example of bounded blend/offset mappings and a hold-toggle stretch feature

The codebase now supports more than “SDK orientation”; it supports a full review cycle including hardware listening, drive retuning, and documented design rationale for every patch.

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
`internal/PatchCppWrapper.cpp`. It is manageable for the current nine patches because all
of them place a live-performance target on param 2, but it will become a design constraint
as soon as a future patch wants expression on a different control. The documented future
path (a virtual `isParamEnabled()` on `Patch.h`) is not yet implemented.

### 2. Playground assets are not yet indexed by family

The repository now holds a documented Plates catalog archive and SpiralCaster creator
examples, but there is no indexed summary of what sonic families they cover, how they
compare to the hand-written effects, or which ones are most useful as listening references
or future inspiration.

### 3. No automated hardware smoke test

`tests/build_effects.sh` validates that the ARM toolchain produces structurally sane `.endl`
binaries (PTCH magic header, file-size spot check), but there is no automated check that
a patch produces expected audio output on the actual device. Functional correctness is
still validated by ear.

---

## Recommended Next Development Moves

### High value

1. Add per-patch expression routing to `Patch.h` and `PatchCppWrapper.cpp` with a safe
default that preserves current behavior. See `docs/endless-reference.md` §5 for the
documented design path.
2. Expand the effect library with a dynamics patch (compressor) or the ProCo Rat distortion,
which is the only remaining shortlisted ElectroSmash candidate without an implementation.

### Medium value

1. Index the Playground archive by effect family and likely DSP category so it is useful
as a listening reference alongside the hand-written patches.
2. Add tiny host-side golden tests for deterministic helper code (coefficient computation,
envelope math) where practical.

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

At review time, `bash tests/check_patches.sh` passes cleanly for all nine custom patches:

- `effects/back_talk_reverse_delay.cpp`
- `effects/bbe_sonic_stomp.cpp`
- `effects/big_muff.cpp`
- `effects/chorus.cpp`
- `effects/klon_centaur.cpp`
- `effects/mxr_distortion_plus.cpp`
- `effects/phase_90.cpp`
- `effects/tube_screamer.cpp`
- `effects/wah.cpp`

`bash tests/build_effects.sh` confirms each produces a valid ARM `.endl` binary with a
correct `PTCH` magic header. Drive-family patches (`tube_screamer`, `klon_centaur`,
`mxr_distortion_plus`, `big_muff`) have been through one hardware-informed retune pass;
see `docs/effects-deep-dive-audit.md` for before/after measurements.

---

## Practical Takeaway

Treat this fork as a stock-SDK-compatible Endless development lab. The most important
conceptual anchors for future work are:

- stay close to upstream in core SDK files
- do real development in `effects/`
- use `docs/` to preserve design intent
- treat Playground artifacts as reference material, not editable source
- branch from `master`
