# Dimension Chorus — Build Walkthrough

**File:** `effects/dimension_chorus.cpp`
**Date:** 2026-06-14
**Filter/algorithm type:** mono ring buffer, shared inverted-pair LFO, dual modulated taps, cross-feed with one-pole highpass and gain-morphed polarity
**Reference circuit / inspiration:** Boss DC-2 Dimension Chorus / Roland SDD-320 Dimension D
**Research notes:** [`docs/dimension-chorus-research.md`](dimension-chorus-research.md)
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

This is the one deliberate exception to the "12 effects, refine the craft,
no new effects" catalog-freeze decision from the prior session — approved
explicitly for this patch because `sthompsonjr` independently building a
DC-2/TriDimension circuit family made a strong case for reopening it. See
[`CLAUDE.md`](../CLAUDE.md) and [`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md)
for that context.

---

## Overview

`effects/chorus.cpp` is a classic modulated-delay chorus: two independent
delay lines, 90°-phase-offset LFOs, equal-power dry/wet crossfade. This
patch is architecturally different, not just re-tuned: one mono delay line,
a *shared* LFO with one tap's modulation exactly inverted (not
independently phased), and stereo width built from cross-feeding each
channel's tap into the other rather than from a dry/wet blend at all — the
real DC-2 has no mix knob. See [`docs/dimension-chorus-research.md`](dimension-chorus-research.md)
for the circuit background this is built from.

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | mono ring buffer, sine LFO, one-pole LP (darkening) + HP (crossfeed), parameter smoother, soft-limit |
| Circuit-modeling approach | Hand-built topology matching the documented DC-2 signal-flow shape (shared inverted LFO, cross-feed), not a WDF circuit port — no literal analog nonlinearity to solve (BBD companding is deliberately not modeled; see the research notes) |
| Fork cross-check | `sthompsonjr`'s new `wdf/Dc2Circuit.h` / `wdf/TscCircuit.h` / `dsp/TriPhaseLfo.h` target the same circuit family — unlicensed, idea-level corroboration only, no code taken |
| Working buffer needed? | Yes — 2048 floats, mono (one ring buffer, not two) |
| State variable count | 1 LFO phase, 1 ring-buffer write index, 4 one-pole filter states (2 LP + 2 HP), 4 param smoothers, 2 bool flags |
| Knob 0 (Left) | `Rate` |
| Knob 1 (Mid) | `Depth` |
| Knob 2 (Right) / Exp pedal | `Width` (crossfeed intensity — stands in for the real pedal's mode selector; there is no literal mix knob) |
| Active LED color | `Color::kDarkCobalt` (Classic) / `Color::kMagenta` (Mono-safe) |
| Bypassed LED color | `Color::kDimBlue` (Classic) / `Color::kDimCyan` (Mono-safe) |
| Working buffer size needed | 2048 floats (~8 KB) |

---

## Decision 1 — Mono Ring Buffer, Not Two Delay Lines

**Goal:** match the real circuit's actual signal path instead of copying
`chorus.cpp`'s shape with different constants.

Both real BBD lines are fed the *same* input audio; they differ only in
their clock's modulation phase. A single mono `dsp::RingBuffer` (2048
floats), written once per sample, read twice at two independently
modulated offsets, is a direct, correct simplification of that — not a
shortcut that loses fidelity. It's also cheaper: `chorus.cpp` spends 4800
working-buffer floats on two stereo delay lines; this patch spends 2048 on
one mono line (~0.09% of the 2,400,000-float working buffer).

**Rejected:** two independent delay lines (à la `chorus.cpp`) — would
require doubling the buffer for no benefit, since the real circuit doesn't
work that way.

---

## Decision 2 — Shared, Inverted LFO Instead of Two Independently-Phased Ones

**Goal:** reproduce the DC-2's "motionless" character — no audible
pitch-wobble, unlike a normal chorus.

One `dsp::SineLfo` produces phase A; phase B is simply `-A`, not a second
oscillator at a different phase offset. Because `depthSamples * lfoA` and
`depthSamples * lfoB` are exact negatives, the two tap delays always sum to
a constant (`2 * kCenterSamples`), matching the real circuit's clock
mechanism. This is also a CPU win: one `sinf` call per sample instead of
two.

**Rejected:** `chorus.cpp`'s 90°-phase-offset dual-sine approach — that's
the correct choice for a classic chorus but produces a different (and,
per the research, less accurate to the DC-2) modulation character.

---

## Decision 3 — Control Layout

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | `Rate` | 0.2–3 Hz, log taper | `0.35f` |
| Mid | 1 | `Depth` | 0–9 ms modulation depth, linear | `0.55f` |
| Right / Exp | 2 | `Width` | crossfeed intensity, linear (0 = exact dry passthrough) | `0.60f` |

The real DC-2 has no mix knob at all — wet is always summed with dry, and
only mode intensity is selectable. Rather than invent a dry/wet blend the
hardware doesn't have, Width (crossfeed depth, the parameter that actually
carries the pedal's character) is the expression-mapped control. This
keeps faith with this repo's "Mix on Right knob, expression-mapped"
convention in spirit — Width *is* the closest analog this patch has to a
wetness control, since at Width=0 the patch is exact dry passthrough.

---

## Decision 4 — Rate Taper

Left knob uses a narrower, slower log taper than `chorus.cpp`'s (0.2–3 Hz
vs. 0.1–5 Hz): `hz = 0.2 * 15^rate`. The real DC-2's modes run calmer than
a typical stomp chorus; a narrower range keeps the whole knob travel
musically useful rather than spending the top quarter on speeds the real
pedal never reaches.

---

## Decision 5 — Crossfeed Gain Morph: Classic vs Mono-safe

**Goal:** give the hold toggle a real, motivated story — not a duplicate of
press, which `effects/README.md`'s cheat sheet already flags as the
weakest hold-mode design in the corpus (`chorus.cpp`'s hold is literally
the same action as press).

The crossfeed gain (`crossGain` in the code) morphs between two values via
a `dsp::ParamSmoother`-driven target, not an instant flip:

- **Classic** (`kClassicCrossGain = 1.8`, over-unity): a stronger, more
  aggressive interference character, closer to the real DC-2's more
  extreme modes.
- **Mono-safe** (`kMonoSafeCrossGain = 0.6`, under-unity): a gentler,
  less divergent character — a practical addition for players who need to
  submix to mono. Not a real DC-2 mode.

**An important honesty note**, because an earlier version of this decision
aimed higher and had to walk it back: the original design intent was for
Classic's over-unity crossfeed gain to reproduce the real hardware's
documented mono-cancellation quirk as a literal, testable property
("summing L+R to mono measurably loses level as Width increases"). That
property does not hold reliably under this implementation's continuously
swept LFO — the relative phase between the direct and cross-fed taps
sweeps through all values over an LFO cycle, so a fixed-sign gain term
does not net-cancel a broadband signal on average (worked through in full
in the top-of-file comment in `effects/dimension_chorus.cpp` and in
[`docs/dimension-chorus-research.md`](dimension-chorus-research.md)'s
closing section). Rather than quietly drop the claim or ship a test that
doesn't actually hold, the acceptance test and this doc were rewritten
around the property the implementation *does* reliably deliver: width-
scaled stereo divergence.

**Rejected:** a simple `-1`/`+1` polarity sign flip on the crossfeed (the
first implementation attempt) — mathematically it has the identical
mono-cancellation limitation as the gain-morph version, so the gain-morph
framing was kept because "stronger vs. gentler crossfeed" is a more
honest, defensible description of what the code actually does.

---

## Decision 6 — LED Design

| State | Color | `Color` enum |
|---|---|---|
| Classic, active | Dark cobalt | `Color::kDarkCobalt` |
| Classic, bypassed | Dim blue | `Color::kDimBlue` |
| Mono-safe, active | Magenta | `Color::kMagenta` |
| Mono-safe, bypassed | Dim cyan | `Color::kDimCyan` |

`Color::kDarkCobalt` had never been used anywhere in the corpus before this
patch (checked against `effects/README.md`'s cheat sheet) — a genuinely
fresh pick for the primary/Classic state.

---

## Decision 7 — State Clearing Strategy

| Event | Clear state? | Reason |
|---|---|---|
| Bypass → active | Yes | avoids stale ring-buffer/filter content bleeding into the re-engaged effect |
| Active → bypass | No | pass-through is already stable; no state to clear |
| Ordinary knob move | No | preserve sweep continuity, matches every other effect in the corpus |
| Classic / Mono-safe toggle | No (smoothed morph instead) | an instant flip would click; `polarityMorph_` glides between the two crossfeed gains over ~30 ms, mirroring `tube_screamer_wdf.cpp`'s `ts9Morph_` pattern |

---

## Implementation Notes

### Working buffer allocation

```cpp
void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
{
    ring_.init(buf.data(), kDelayLen);  // kDelayLen = 2048
    ring_.reset();
    ready_ = true;
}
```

One mono ring buffer, not two. `ready_` gates `processAudio` the same way
`chorus.cpp`'s `if (!delayL_) return;` null-check does — `init()`
deliberately does not touch `ring_` itself, since the host calls
`setWorkingBuffer` and `init()` in an order this patch doesn't control.

### CPU budget estimate

Per sample: one `sinf` (LFO tick) and one `powf` (the Rate taper, recomputed
every sample — matching the corpus convention of stepping smoothers
per-sample, see `effects/tube_screamer_wdf.cpp`'s `drive_.process()`
pattern), two fractional ring-buffer reads (a handful of multiply-adds
each), two one-pole LP calls (darkening) and two one-pole HP calls
(crossfeed) — each one or two multiply-adds — and two `softLimit` calls
(branch, plus an occasional `tanhf` only on the overshoot path). No
measured hardware cycle data exists for this patch yet — see
[`docs/cycle-budget.md`](cycle-budget.md).

### Mono ring buffer vs. mono-cancellation testing

Because this patch `#include`s cleanly into a single translation unit (no
other file defines `Patch::getInstance()` inside the same TU), the
acceptance test in `tests/dimension_chorus_acceptance_test.cpp` includes
`effects/dimension_chorus.cpp` directly rather than linking a separately
compiled object — see that file's header comment for why, and for the
full property it protects (stereo-divergence growth with Width, not
mono-sum cancellation — see Decision 5 above).

**One-time comparison against `chorus.cpp`** (not committed as a permanent
test — recorded here as design-rationale evidence): measuring
stereo-difference (L−R) RMS across each patch's own knob sweep, using an
identical methodology (440 Hz test tone, 100 ms settle + 100 ms measure
per point):

| Knob value | `chorus.cpp` (Mix) | `dimension_chorus.cpp` (Width) |
|---|---|---|
| 0.00 | 0.000000 | 0.000213 |
| 0.25 | 0.126698 | 0.271690 |
| 0.50 | 0.242100 | 0.421817 |
| 1.00 | 0.274750 | 0.827078 |

`chorus.cpp`'s Mix-driven divergence **plateaus early**: from 0.5 to 1.0 it
grows only ~14% (0.242 → 0.275), because Mix is a wetness blend layered on
top of a roughly fixed decorrelation baseline from the two 90°-offset delay
lines. `dimension_chorus.cpp`'s Width-driven divergence keeps growing
substantially across the same range — nearly doubling from 0.5 to 1.0
(0.422 → 0.827) — because Width directly scales the crossfeed term that
*generates* the divergence, rather than blending a pre-existing amount of
it. This is the concrete, measured evidence that Width is architecturally
a different kind of control than Mix, not just a renamed copy.

---

## Testing

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
bash scripts/build_effects.sh --effect dimension_chorus
g++ -std=c++20 -O2 -fsingle-precision-constant -Wall -Wextra -I source \
    tests/dimension_chorus_acceptance_test.cpp -o /tmp/dc_accept && /tmp/dc_accept
```

**Manual listening checklist:**

- At Width=0, output is exact dry passthrough — no audible effect at all.
- Sweeping Width from 0 to 1 should feel like widening/interference
  building, not like a volume-of-wet-signal fade (contrast against
  `chorus.cpp`'s Mix knob, which does fade wetness).
- Classic vs Mono-safe (hold toggle) should be audibly different in
  intensity, with Classic feeling more aggressive/interference-heavy.
- Rate sweep should feel calm and "motionless" (no obvious pitch wobble)
  compared to `chorus.cpp`'s Rate knob at a similar setting.
- Bypass and hold-toggle are pop-free.
- Check in mono (sum L+R): Classic should sound narrower/more filtered
  than Mono-safe at the same Width — the qualitative character difference
  the design targets, even though a strict monotonic RMS-loss claim isn't
  made (see Decision 5).

---

## Known Limitations

- **No literal mono-cancellation guarantee.** See Decision 5. The
  crossfeed topology is inspired by the real hardware's documented
  behavior but does not reproduce it as a provable, swept-LFO-robust
  property. Documented rather than silently smoothed over.
- **BBD companding not modeled.** A deliberate choice, not an oversight —
  see [`docs/dimension-chorus-research.md`](dimension-chorus-research.md).
- **Secondary sources only.** The circuit research is corroborated across
  multiple independent write-ups but not verified against a primary
  schematic (several were unreachable during research).
- **No measured hardware cycle data.** Like every other patch in this
  corpus today — see [`docs/cycle-budget.md`](cycle-budget.md).

---

## Future Improvements

1. If hardware cycle data ever shows meaningful headroom, consider a
   proper polyphase or all-pass-based decorrelation stage for a shot at a
   genuinely swept-LFO-robust mono-cancellation property — the naive
   approach here hit a real DSP-theoretic wall (Decision 5), but a more
   sophisticated construction (e.g. borrowing from Haas-effect widener
   literature) might not.
2. A discrete 4-mode intensity selector (matching the real DC-2's Modes
   1–4) instead of a continuous Width knob could be a closer-to-hardware
   alternative control scheme, at the cost of losing continuous
   expression-pedal sweep — worth a listening comparison before committing
   either way.

---

## Related Files

- `effects/dimension_chorus.cpp`
- `effects/chorus.cpp` (the architectural contrast case)
- `effects/tube_screamer_wdf.cpp` (`ts9Morph_`-style smoothed hold-toggle pattern reuse)
- `tests/dimension_chorus_acceptance_test.cpp`
- `source/dsp/ring_buffer.h`, `source/dsp/lfo.h`, `source/dsp/one_pole_filter.h`
- `docs/dimension-chorus-research.md`
- `docs/fork-comparisons/sthompsonjr-wdf.md`
- `docs/circuit-to-patch-conversion.md`
- `docs/endless-reference.md`
