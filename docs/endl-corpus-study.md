# What 42 Compiled Patches Say About Writing Endless Patches

**Date:** 2026-08-23
**Corpus:** every `.endl` in this repo — 5 Polyend factory plates
(`playground/polyend_plates/`), 23 Playground-generated community patches
(`playground/examples/SpiralCaster_Examples/`), and this repo's own 14 builds
(`effects/builds/`).
**Tooling:** [`scripts/endl_inspect.py`](../scripts/endl_inspect.py),
[`scripts/endl_analyze.py`](../scripts/endl_analyze.py). Raw output in
`build/endl_analysis/summary.{json,md}`.

This is a structural and statistical study. Polyend owns the factory plates and
this repo claims no rights over them (see
[`playground/polyend_plates/README.md`](../playground/polyend_plates/README.md));
nothing recovered here has been transcribed into `effects/`.

---

## Method, and why you should believe the negative results

Third-party `.endl` files carry no symbols. To identify library routines in
them, `endl_analyze.py` builds fingerprints from *our* builds, where
`build/<name>.elf` gives ground-truth addresses and sizes, and matches those
fingerprints against unlabeled images. A fingerprint is the set of 4-byte words
present in a routine in every one of our 14 builds, minus words appearing in
more than two different routines. Literal-pool constants dominate, and those are
position-independent.

Two controls make the results trustworthy:

- **No false positives.** Run against our own binaries, the detector reports
  exactly the routines `arm-none-eabi-nm` says are there, and nothing else.
- **Cross-build matching demonstrably works.** `patch_init` — from
  `internal/patch_main.c`, shared by every image built against this ABI —
  matches third-party plates at **0.938**. So the method does find shared code
  across independently-built images.

That second control is what makes the central negative result meaningful.

---

## Finding 1 — Polyend's patches contain no newlib transcendentals. Ours are made of them.

| Group | n | newlib libm routines detected |
|---|---|---|
| Polyend factory plates | 5 | **none, in any image** |
| Playground (community) | 23 | **none, in any image** |
| This repo | 14 | 5–13 routines each |

Against the 0.938 `patch_init` control, the best libm fingerprint score anywhere
in a third-party image is **0.111** (`expf` in `Wax.endl`) — noise.

An independent argument rules out a false negative for the smaller plates
without relying on fingerprinting at all: newlib's `__ieee754_powf` is 1,668
bytes and `__kernel_rem_pio2f` is 1,596 bytes. `Malleus_Fuzz.endl`'s **entire
image is 3,012 bytes** and `Fault Line.endl`'s is 2,632. Those routines cannot
be in there.

What that costs us, measured from our own symbol tables:

| Effect | image | libm bytes | libm % | effect code |
|---|---|---|---|---|
| wah | 8,472 | 6,062 | **83.4%** | 1,126 |
| phase_90 | 7,680 | 5,102 | 79.1% | 1,262 |
| chorus | 7,760 | 5,202 | 79.0% | 1,302 |
| dimension_chorus | 9,596 | 6,546 | 78.9% | 1,666 |
| big_muff | 9,616 | 6,062 | 72.6% | 2,206 |
| bbe_sonic_stomp | 13,284 | 3,114 | 51.7% | 2,826 |
| **all 14 combined** | — | **73,100** | **69.3%** | 31,238 |

Mean 5,221 bytes of newlib libm per effect. `wah.cpp` compiles to 1,126 bytes of
wah wrapped in 6,062 bytes of math library.

**Image size is not the problem** — every effect sits at 1–2% of the 512 KB
budget, and that is not going to change. The problem this points at is CPU cost.
`sinf`/`cosf` reach newlib's `__ieee754_rem_pio2f` argument reduction and
`powf` reaches `__ieee754_powf`; these are branch-heavy, hundreds-of-cycles
routines, and several of our effects call them per sample.
[`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md) §6
already warns about per-sample `powf` on its own reasoning. This corpus says
something stronger: **the platform's own patches do not use these routines at
all.**

**Honest limit on this claim:** no cycles were measured.
[`docs/cycle-budget.md`](cycle-budget.md)'s table is still empty and this study
does not fill it. What the corpus establishes is a strong prior about platform
norms, not a demonstrated regression in any of our effects. It is also not
established *what* the third-party patches do instead — polynomial
approximations, table lookups, or simply DSP that needs no transcendentals are
all consistent with the evidence. Their FPU instruction ratios (0.11–0.40) sit
in the same range as ours (0.22–0.39), so they are certainly not avoiding
floating-point work in general.

**Suggested practice:** treat a newlib transcendental in a per-sample path as a
cost to justify, not a default. Where one is needed for a control-rate value,
compute it once per block — the pattern `dimension_chorus.cpp` already uses.
Where one is needed per sample, a bounded polynomial or table approximation is
what the rest of the platform appears to do.

## Finding 2 — Polyend's SDK exposes param name/unit as virtuals. The public SDK throws the slot away. Nobody appears to use it.

This finding was initially written as "every third-party patch names its knobs
and none of ours do." **That was wrong and is corrected here**, because the
detector that produced it measured the wrong thing.

What is solid: our images and every third-party image differ completely in how
`agent_get_param_name` / `agent_get_param_unit` are compiled.

Ours, from `internal/PatchCppWrapper.cpp`, is an inlined do-nothing stub:

```
800019e4:  cbz   r3, 0x800019ea
800019e6:  movs  r3, #0
800019e8:  strb  r3, [r2, #0]
800019ea:  bx    lr
```

`Wax.endl`'s — and, byte-for-byte identically, `Malleus_Fuzz.endl`'s — spills
its arguments, calls a thread-safe local-static guard (`dmb ish`), loads a
vtable pointer and tail-calls through slot +24:

```
80002c10:  push  {r4, lr}
80002c14:  strd  r2, r1, [sp, #8]
80002c1a:  bl    0x80002ae8      ; singleton accessor with guard
80002c1e:  ldr   r4, [r0, #0]    ; vtable
80002c22:  ldr   r4, [r4, #24]   ; slot 24
80002c30:  bx    ip              ; tail-call the virtual
```

That is a **virtual dispatch thunk**, structurally the same shape as the rest of
our own wrapper's forwarding functions. So: **Polyend's internal C++ SDK has
virtual methods for parameter name and unit that the public FxPatchSDK does not
expose at all.** The ABI slot is real and their SDK wires it to the patch class;
ours discards it before a patch author can reach it.

**What this does *not* show — and the reason the original claim was withdrawn:**
a thunk reveals nothing about what the virtual behind it returns. And scanning
all five factory plates for printable text finds **no knob-name strings
whatsoever** — no "Drive", "Mix", "Tone", "Level", nothing. The printable byte
runs in those images are coincidental opcodes (`pG` is `70 47`, i.e. `bx lr`).

So the most likely reading is that Polyend's plates forward to a virtual that
returns an empty string, or that display names are carried outside the binary
entirely (the Plates web catalog has names and descriptions the `.endl` does
not). **There is no evidence in this corpus that any patch supplies a knob name,
or that the firmware displays one.**

The slot is still worth wiring up on our side — it is free, additive, and
restores API surface the ABI defines and Polyend's own SDK uses. It is done in
[`docs/param-metadata-implementation.md`](param-metadata-implementation.md).
But it is speculative plumbing, not a feature demonstrated to work.

## Finding 3 — The `bss_size` difference is a build artifact, not a latency signal

| Group | `bss_size` values |
|---|---|
| Polyend factory plates | 4, 112, 308 |
| Playground (community) | 4, 112, 128, 140, 308, 1,192, 3,556 |
| This repo | **5, for all 14** |

This looked like it might mean Polyend keeps hot state in fast on-chip memory
while we push everything into the 9.6 MB working buffer that
[`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md) §6
infers is external RAM. **It does not, and the hypothesis is withdrawn.**

Our 5 bytes of `.patch_bss` are the SDK's own two variables and nothing else:

```
80001ed8 00000004 b ctors_initialized.0
80001edc 00000001 B error
```

The patch object is not missing — `Patch::getInstance()` returns `0x80001eac`,
which lies inside `.patch_image`. `internal/patch_imx.ld` folds `.data` into
`.patch_image`, so our patch state is 44 bytes of initialized data shipped in
the image rather than zero-initialized data reserved after it.

Both `.data` and `.bss` land in the same single 512 KB region at
`PATCH_LOAD_ADDR`. Moving state between them changes nothing about access
latency. The difference is where the compiler placed initialized versus
zero-initialized objects, and it carries no performance meaning.

The recurring values 4 / 112 / 308 across *both* third-party groups do suggest
the factory plates and the Playground compiler share a build harness, which is
mildly interesting and entirely unactionable.

## Finding 4 — Our images are the largest, and that is entirely Finding 1

| Group | `image_size` min / median / max |
|---|---|
| Polyend factory plates | 3,012 / 3,948 / 12,984 |
| Playground (community) | 2,632 / 5,764 / 10,044 |
| This repo | 5,460 / **9,596** / 13,284 |

Our median image is ~2.4x the factory plates'. Subtract the libm that only we
link and our effect code is 1,126–4,990 bytes, squarely inside the third-party
range. There is no separate code-bloat problem to chase.

---

## What actually changes as a result

1. **Newlib transcendentals in per-sample paths need justification.** Folded
   into [`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md).
2. **Param name/unit virtuals are now exposed** — the ABI slot exists and
   Polyend's own SDK uses it, though no corpus evidence shows any patch
   supplying a name or the firmware displaying one.
3. **Nothing about BSS or image size.** Both looked like findings and neither
   survived being checked.

## Reproducing

```bash
python3 scripts/endl_analyze.py --rebuild-signatures
```

Rebuilding signatures requires `build/*.elf` from `bash tests/build_effects.sh`.
Cached signatures live in `build/endl_analysis/libm_signatures.json`.

The corpus here is the 28 third-party binaries committed to the repo.
`scripts/sync_polyend_plates.sh` can fetch the full ~45-plate catalog for a
larger sample; it could not run in the environment this study was done in
(`polyend.com` is blocked by the sandbox network policy), and the scripts take
paths, so re-running over a fuller corpus needs no code change.

## See also

- [`docs/endl-binary-format.md`](endl-binary-format.md) — the format spec
- [`docs/cycle-budget.md`](cycle-budget.md) — still-empty measurement table this
  study deliberately does not pretend to fill
- [`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md)
- [`docs/hardware-cycle-measurement-howto.md`](hardware-cycle-measurement-howto.md)
