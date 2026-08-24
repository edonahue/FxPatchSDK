# DSP idioms in Polyend's own patches

**Date:** 2026-08-23
**Source:** disassembly of the five patches listed in
[`README.md`](README.md), plus an instruction census across all 19 factory and
own-build images.

[`docs/endl-corpus-study.md`](../endl-corpus-study.md) established that no
third-party patch links a newlib transcendental, and left open what they do
instead. This answers that.

---

## Instruction census

Whole-image linear sweep, identical treatment for every binary. Literal pools
decode as noise, so read the ratios, not the absolutes.

| | insns | `bl` /1k | `vfma` /1k | `vmaxnm` | `vminnm` | `vabs` | `vdiv` /1k |
|---|---|---|---|---|---|---|---|
| Polyend factory (5) | 10,698 | 24.9 | 26.5 | **66** | **76** | **42** | 3.8 |
| This repo (14) | 43,281 | 21.2 | 23.4 | **0** | **0** | **0** | 6.4 |

Three complete separations, not differences of degree.

## Finding A — the nonlinearity is `x / (1 + |x|)`, not `tanhf`

`Malleus_Fuzz`'s clipping stage, straight out of the disassembly:

```
vabs.f32  s17, s1          ; |x|
vadd.f32  s17, s17, s12    ; |x| + 1.0
vdiv.f32  s12, s1, s17     ; x / (1 + |x|)
```

Three instructions and one divide. It appears **twice in cascade**, giving a
softer knee than one stage without touching a library. This is the whole reason
their images contain no `tanhf` and 42 `vabs` instructions to our zero — and
it is the direct answer to the question the corpus study left open.

Ours calls `tanhf` (`dsp::softLimit`), which reaches newlib's `expm1f`/`scalbnf`.

## Finding B — asymmetry is branchless, via predicated literal loads

The fuzz's even-harmonic character comes from treating the two half-cycles
differently, and it costs no branch:

```
vcmpe.f32 s13, #0.0
vmrs      APSR_nzcv, fpscr
ite       ge
vldrge    s1, [pc, #804]   ; positive-half gain
vldrlt    s1, [pc, #804]   ; negative-half gain
vmul.f32  s1, s13, s1
```

The same `ite`/`vsel` pattern selects the alternate voice from the hold-toggle
flag (`ldrsb r3, [r0, #68]` then `vseleq.f32`) — a mode switch with no branch in
the per-sample path at all.

## Finding C — clamping is one instruction, and ours is six

`vmaxnm`/`vminnm` are FPv5 single-cycle IEEE-754 min/max. Factory plates use 142
of them. We use none, because of how our clamp helpers are written — and,
measured with the Makefile's exact flags, because of `-fno-builtin`:

| written as | compiles to |
|---|---|
| `if (v < 0) return 0; if (v > 1) return 1; return v;` | 6 insns, **with a branch** |
| `fminf(fmaxf(v, 0.0f), 1.0f)` | `bl fmaxf` + `b.w fminf` — **two real calls** |
| `__builtin_fminf(__builtin_fmaxf(v, 0.0f), 1.0f)` | `vmaxnm` + `vminnm` — **2 insns, branchless** |

The middle row is the trap: `-fno-builtin` is in `COMMON_FLAGS`, so the obvious
fix is the worst of the three. Only the explicit builtins work under our flags.

`clampSigned(v, l)` shows the same shape: 9 instructions with a branch, versus
`vmaxnm`/`vminnm` and nothing else.

Every effect in this repo has a `clamp01` and most have a `clampSigned`, called
one to several times per sample. **This is not applied** — it is a change to
shared code across 14 effects, and it alters NaN behavior (the `if` chain
propagates NaN; `vmaxnm` returns the non-NaN operand, so a NaN would become a
bounded value and stop tripping `effect_probe.cpp`'s detector). Worth doing
deliberately, with the probe re-run, rather than as a drive-by.

## Finding D — one-poles are FMA, and state is dense

The recurring three-instruction one-pole, from `Yield_reverse_delay`:

```
vldr      s24, [r0, #116]   ; state
vsub.f32  s0,  s11, s24     ; x - y
vfma.f32  s24, s0,  s7      ; y += (x - y) * a
vstr      s24, [r0, #116]
```

We already emit `vfma` at a similar rate (23.4 vs 26.5 per 1k), so this is
confirmation rather than a gap.

Their state is packed in the object and read by fixed offset — `[r0, #116]`,
`#120`, `#128`, up to `#168` in `Yield_reverse_delay` — and input is streamed
with post-incrementing `vldmia ip!, {s11}`. Coefficients are all hoisted into
registers before the loop (a block of ~12 `vldr` from PC-relative literals), the
same per-block hoisting this repo already practises.

## What this changes

1. **`x/(1+|x|)` belongs in `source/dsp/`** as a cheap alternative to
   `dsp::softLimit`'s `tanhf`, for the safety stage and for drive effects where
   the exact `tanh` curve is not the point. Two effects would need it before it
   meets this repo's own extraction bar.
2. **Clamp helpers should use `__builtin_fminf`/`__builtin_fmaxf`** — 6 insns
   plus a branch down to 2, in code every effect runs several times per sample.
   Flagged, deliberately not applied; see Finding C.
3. **Branchless mode selection** via `vsel`-friendly ternaries is worth
   preferring over `if` in per-sample paths.

None of this is cycle-measured. [`docs/cycle-budget.md`](../cycle-budget.md) is
still empty, and instruction counts are not cycles — but "6 instructions and a
branch" versus "2 instructions" does not need a profiler to be worth acting on.

## See also

- [`README.md`](README.md) — recovery method and validation
- [`docs/endl-corpus-study.md`](../endl-corpus-study.md) — the corpus statistics
- [`docs/patch-authoring-best-practices.md`](../patch-authoring-best-practices.md) §6
