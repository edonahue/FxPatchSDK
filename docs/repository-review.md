# Repository Review: Current State

**Reviewed:** 2026-08-24, against `master` at the merge of PR #12.
**Supersedes:** the 2026-04-07 review, which described a three-patch repo and a
branch situation that no longer exists. Nothing from it survived contact with
the current tree; it was rewritten rather than patched.

This is item #1 in [`CLAUDE.md`](../CLAUDE.md)'s reading order, so it aims to
answer one question: *what is actually here right now?*

---

## Shape of the repo

| | |
|---|---|
| Custom effects | **14** in `effects/*.cpp`, plus one external reference in `effects/examples/reverb.cpp` |
| Shared DSP primitives | **10** header-only files in `source/dsp/`, with **9** unit tests in `tests/dsp/` |
| Compiled `.endl` binaries | **42** — 14 our own (untracked build output), 23 community Playground bundles, 5 Polyend factory plates |
| Documentation | 35 top-level docs in `docs/`, plus `fork-comparisons/`, `reverse-engineering/`, `templates/` |
| Desktop host | JUCE VST3 / LV2 / Standalone wrapper in `vst/` — built and working, experimental |
| Commits on `master` | 101 |

## Branches

`master` is the only long-lived branch and the base for all work. Development
happens on short-lived branches named in the session instructions; they are
deleted once merged. See [`CLAUDE.md`](../CLAUDE.md)'s operational notes.

For fork and upstream history — which is a different question from branch state
— use
[`fork-comparisons/upstream-and-forks.md`](fork-comparisons/upstream-and-forks.md).
Upstream `polyend/FxPatchSDK` is dormant and this fork is caught up with it.

## What this fork adds over upstream

Upstream is the SDK skeleton plus one example. This fork keeps the stock API and
ABI unchanged and adds:

- **14 patches**, each with a build walkthrough in `docs/` explaining the *why*
  of its control laws, not just the what.
- **A shared DSP layer** (`source/dsp/`) extracted only from proven duplicates —
  the standing rule is that a primitive needs two or more real users before it
  earns a file.
- **Host-side validation** that runs without hardware: syntax/lint with `-Werror`
  parity against the real ARM flags, per-primitive unit tests, real ARM `.endl`
  builds with a RAM-budget and header gate, and behavioral probe sweeps with
  NaN/Inf detection and THD/spectral analysis.
- **Binary analysis tooling** (`scripts/endl_inspect.py`, `scripts/endl_analyze.py`)
  that reads compiled `.endl` images directly — header validation, entry-point
  and vtable recovery, library-routine fingerprinting.
- **A desktop audition path** via `vst/`.

## Validation surface

```bash
bash tests/check_patches.sh    # syntax + lint (-Werror) and the DSP unit tests
bash tests/check_arm_build.sh  # real ARM .endl builds, header + RAM + stack gates
bash tests/analyze_effects.sh  # probe sweeps, THD/spectral, control-law metrics
```

All three run with no hardware attached. What they cannot tell you is how a patch
*sounds* — see "Known gaps".

## Known gaps

These are real and currently open. Closed gaps have been removed from this list
rather than left in place looking urgent.

1. **No measured cycle data.** [`cycle-budget.md`](cycle-budget.md)'s per-patch
   and per-primitive tables are still empty. The 15k cycles/sample ceiling is a
   rule of thumb inherited from the `sthompsonjr` fork, not a Polyend figure.
   [`hardware-cycle-measurement-howto.md`](hardware-cycle-measurement-howto.md)
   describes the DWT approach but is unverified, and requires opening the pedal
   and attaching an SWD probe.
2. **No hardware listening loop in this environment.** Every patch and every
   change is validated on the host only. Several claims in this repo —
   parameter-name display being the most recent — are inference from the ABI and
   from Polyend's own binaries, not observation.
3. **No CI.** There is no `.github/workflows/`, despite four runnable validation
   scripts. Everything is run by hand.
4. **No persistent storage API.** Patches cannot save state across power cycles;
   upstream issue #1 asks for it and remains unanswered.
5. **Expression is hardwired to param 2** in `internal/PatchCppWrapper.cpp`. A
   per-patch `isParamEnabled()` routing API is a known possibility that this repo
   has deliberately declined — see `CLAUDE.md`.

## Where to go next

Follow [`CLAUDE.md`](../CLAUDE.md)'s reading order rather than this document.
This file records state; the reading order records how to work.
