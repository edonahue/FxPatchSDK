# Host Cache-Distance Probe: Does This Technique Even Apply Here?

**Date:** 2026-08-23.
**Status:** measured, and the result is a clean **null** — documented
honestly below, including why. Does not modify
`effects/back_talk_reverse_delay.cpp` — a standalone, additive experiment.

## The question

`docs/patch-authoring-best-practices.md` §6 infers the working buffer is
very likely external SDRAM/PSRAM, not on-chip SRAM, from linker-script
math (a 512 KB on-chip patch-image region plus the 9.6 MB working buffer
together exceed any single Cortex-M7 variant's on-chip SRAM). That
inference implies a read far from the write head should pay a materially
bigger latency cliff than an on-chip cache miss.
`effects/back_talk_reverse_delay.cpp` is the single largest exposure to
this in the corpus — its reverse-chunk length reaches ~57,600 samples
behind the write head, about 12x further than the "100 ms delay" example
already used to illustrate the concern.

No real Cortex-M7 hardware access exists in this environment (see
`docs/hardware-cycle-measurement-howto.md`). Before concluding "nothing
can be measured until hardware access exists," this experiment asked a
narrower, answerable question: does *this host's own* cache hierarchy show
a distance-dependent read cost at the sizes `back_talk_reverse_delay.cpp`
actually uses? If so, the same host-cycle-ratio technique
`docs/aliasing-oversampling-experiment.md` used for a *different* question
(relative algorithm cost, not memory latency) might extend usefully here
too, as at least an ordering signal while real hardware data doesn't
exist.

## Method

- **Harness:** [`tests/host_cache_distance_probe.cpp`](../tests/host_cache_distance_probe.cpp).
  Allocates a `kDelayLen = 131072`-sample buffer — the exact size
  `back_talk_reverse_delay.cpp` uses, not an arbitrary round number — fills
  it (a real delay line is continuously written, not a static array), then
  reads at a fixed distance behind a fixed write-head position, advancing
  both together for `4096` reads per trial, `__rdtsc()`-timed, min-of-50
  trials (matching `aliasing-oversampling-experiment.md`'s own min-of-N
  methodology).
- **Four distances probed:** 4,800 samples (the "100 ms" example),
  24,000 (midpoint), 57,600 (`back_talk_reverse_delay.cpp`'s own maximum
  reverse-chunk reach), and 131,071 (`kDelayLen - 1`, the largest distance
  the buffer allows at all — included specifically as a sanity check that
  the probe would detect *something* if the host's cache hierarchy ever
  ran out of room for this buffer size).

## Results

| Distance (samples) | Min cycles/trial | Cycles/read |
|---|---|---|
| 4,800 | 6,771 | 1.65 |
| 24,000 | 6,771 | 1.65 |
| 57,600 | 6,771 | 1.65 |
| 131,071 (max) | 6,772 | 1.65 |

**No measurable distance-dependent cost, at any distance, including the
maximum the buffer allows.** This is not noise or an artifact of too
narrow a distance range — the sanity-check point at the full buffer size
still shows nothing.

## Conclusion

The result is a genuine, clean null — and the reason is straightforward
once measured: `back_talk_reverse_delay.cpp`'s buffer is 512 KB
(`131072 * 4 bytes`), which comfortably fits inside a single core's L2
cache on essentially any modern x86 host, let alone L3. Every distance
probed here is cache-resident throughout, so this host's memory hierarchy
cannot distinguish "4,800 samples behind" from "131,071 samples behind" —
they're all equally close, from the cache's point of view.

**This means the host-cycle-ratio technique that worked for
`aliasing-oversampling-experiment.md` does not usefully extend to this
question.** That experiment measured *relative algorithm cost* (one
nonlinearity vs. a 2x-oversampled version of the same nonlinearity) —
a question host CPU cycles answer just as validly as Cortex-M7 cycles,
since both chips pay roughly proportional costs for the same arithmetic.
*This* question is about *memory hierarchy*, where the host and the
target are qualitatively different: a modern x86 has multi-megabyte
on-chip caches that make a 512 KB buffer trivially resident, while the
Cortex-M7's on-chip SRAM is inferred to be too small to hold the working
buffer at all (see the linker-script math cited above) — the entire
premise of the concern this experiment tried to probe is a scale
difference the host doesn't share.

**No substitute for real hardware measurement exists for this specific
question.** `back_talk_reverse_delay.cpp` remains the first patch to
profile if/when DWT/debug-probe access to a real Endless ever exists
(`docs/hardware-cycle-measurement-howto.md`); this experiment's honest
contribution is confirming that no available host-side technique can
answer the question in the meantime, rather than leaving that untested
and assumed.

## Reproducing

```bash
g++ -std=c++20 -O2 -fno-exceptions -fno-rtti -fsingle-precision-constant \
    tests/host_cache_distance_probe.cpp -o build/host_cache_distance_probe
build/host_cache_distance_probe
```

## See also

- [`tests/host_cache_distance_probe.cpp`](../tests/host_cache_distance_probe.cpp)
- [`docs/patch-authoring-best-practices.md`](patch-authoring-best-practices.md) §6 —
  the external-RAM inference this experiment tried to extend a measurement
  technique to
- [`docs/back-talk-reverse-delay-build-walkthrough.md`](back-talk-reverse-delay-build-walkthrough.md) —
  the patch this experiment is about, with the same reasoning threaded
  into its own "CPU budget estimate" section
- [`docs/aliasing-oversampling-experiment.md`](aliasing-oversampling-experiment.md) —
  the precedent this experiment's methodology follows, and the case where
  the host-cycle-ratio technique *does* apply (relative algorithm cost,
  not memory latency)
- [`docs/hardware-cycle-measurement-howto.md`](hardware-cycle-measurement-howto.md) —
  the only path to a real answer for this question
- [`docs/cycle-budget.md`](cycle-budget.md) — per-patch cycle-cost table,
  now pointing at `back_talk_reverse_delay.cpp` as the first patch to
  profile on real hardware
