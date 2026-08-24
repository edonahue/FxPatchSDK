# Patch Authoring Best Practices for the Polyend Endless

This document is the canonical "how to handcraft a good Endless patch" reference.
It crystallises lessons learned across the fourteen effects in
[`effects/`](../effects), the primitives extracted in
[`source/dsp/`](../source/dsp), and the per-patch walkthroughs in
[`docs/`](.). Read it once before authoring a new effect; refer back to it
when something feels harder than it should.

It is also this repo's positioning statement: a craft-and-best-practices
reference for handcrafting Endless patches in C++, as a deliberate
counterpoint to the Polyend Playground AI-generation flow. The argument for
why hand-authored patches still beat prompt-and-pray on this hardware is in
§8 at the end.

## 1. The Endless mental model

The pedal is an ARM Cortex-M7 at 720 MHz, running stereo audio at exactly
**48000 Hz, single-precision floats, in-place buffers**. A patch is a
single `Patch` subclass whose entry points are:

- `init()` — called once on load. Set defaults, zero state, do nothing
  expensive.
- `setWorkingBuffer(std::span<float, kWorkingBufferSize>)` — called once,
  hands you 2.4M floats (9.6 MB) of scratch storage. Use it for delay lines,
  not for ephemeral state.
- `processAudio(left, right)` — the hot loop. Both spans are the same length,
  same memory in and out.
- `setParamValue(idx, value)` — three normalized knobs (`0..1`), runs on the
  audio thread.
- `handleAction(idx)` — footswitch press (`0`) or hold (`1`).
- `getStateLedColor()` — one of sixteen named colors.

That is the entire control surface. There is no MIDI, no preset store, no
persistent storage, no runtime sample rate, no second effect slot. Design
inside those walls. The hard constraints are spelled out in
[`endless-reference.md`](endless-reference.md) §3 / §7 / §8 — don't
restate them in patch files; assume them.

The corollary: an Endless patch is a tiny piece of software with a *very*
narrow input/output contract. That is a feature, not a bug. The narrow
contract is what makes it possible to design control laws that feel
considered rather than arbitrary, and it is the part the Playground prompt
flow struggles to honour at this hardware scale.

## 2. Control-law design

The three knobs and the footswitch are the *entire* user surface. They have
to carry every voicing decision the patch wants the player to be able to
make. Every effect in this repo follows the same handful of conventions.

### Knob assignment

Use the **Left / Mid / Right** physical order. Within that order, follow
guitar-pedal idiom where one exists:

- **Drive-style effects** (Tube Screamer, Klon, MXR, Big Muff, harmonica):
  Drive / Tone / Level. The audio you hear hardest first (gain) goes left;
  voicing in the middle; output trim on the right.
- **Modulation effects** (chorus, phase 90): Rate / Depth / Mix, in that
  order.
- **Delay-style effects** (back_talk reverse): the temporal control on the
  left (Speed / chunk window), feedback in the middle, Mix on the right.
- **Filter effects** (wah): the *primary* sweep parameter is what the player
  reaches for, which is usually the expression pedal — so the Right knob
  carries the sweep position (and so does param 2).

When in doubt, **Mix on the Right knob, expression-mapped**. Five of the
fourteen effects expose a dry/wet blend at all, and four of those put it
there: `back_talk_reverse_delay` (`Mix`), `big_muff` and `big_muff_wdf`
(`Blend`), and `chorus` (`Mix`). A player who finds the blend on one of them
knows where it lives on the others.

`wah.cpp` is the exception that proves the rule: its `Mix` is on the **Left**
knob, because its Right knob carries the expression-driven sweep position,
which is the control a player actually needs underfoot. When the two
conventions collide, the expression lane wins.

`dimension_chorus.cpp` is a separate documented case — the real hardware it's
modeled on has no mix knob at all, so its Right knob carries Width (crossfeed
intensity)
instead; see `docs/dimension-chorus-build-walkthrough.md`'s Decision 3 for
why that's a deliberate divergence, not an oversight.

### Knob taper

Knob input is linear `0..1` from the hardware. Map it into the parameter the
DSP actually needs with a *deliberate* taper:

- **Drive / gain controls** want a log-ish taper so the bottom of the knob
  is usable and the top has enough range. `powf(value, k)` with `k` in
  `1.5..2.5` is the standard move; see `effects/mxr_distortion_plus.cpp`'s
  Distortion control for a worked example.
- **Frequency controls** want exponential mapping: `f0 * powf(ratio, value)`
  so an octave traversal is constant on the knob. See
  `effects/wah.cpp` and `effects/phase_90.cpp` for log-frequency sweeps.
- **Mix / level controls** are often happiest linear in dB, *not* linear in
  amplitude. Equal-power crossfade
  ([`dsp::equalPower`](../source/dsp/crossfade.h)) keeps perceived loudness
  constant across the sweep — use it for any dry/wet knob.
- **Time controls** (delay length, chorus depth) are usually fine linear
  unless the range spans more than ~2 octaves of time, in which case
  exponential is kinder.

Always clamp the knob first (`if (v < 0) v = 0; if (v > 1) v = 1;`) before
any non-linear mapping. Hardware values stay in range in practice, but a
clamp at the door costs almost nothing and prevents an out-of-domain
exception from a downstream `powf(value, k)`.

### Expression pedal

In this fork, expression is hard-wired to **param 2 (Right)** by
[`internal/PatchCppWrapper.cpp`](../internal/PatchCppWrapper.cpp). Design
the Right knob to be the parameter the player is most likely to want to
sweep with their foot — usually Mix or the primary expressive parameter
(wah position, tone). The Right knob is *ignored* by the firmware when the
expression pedal is connected, so do not put a parameter on Right that the
player needs to set and leave alone.

### Footswitch conventions

Two actions: **press** and **hold**. The conventions every effect in this
fork follows:

- **Press = bypass toggle.** Always. Even if a patch has no alternate
  voicing, press still toggles bypass.
- **Hold = alternate voice / mode toggle.** For variants that have a Tone
  Bypass (Big Muff), a TS9 vs TS808 sibling (Tube Screamer), a Vox vs
  Crybaby variant (wah), a script-mod vs block-logo variant (Phase 90), or
  an Open vs Cupped variant (harmonica) — Hold is where it lives.
- **No third action.** The pedal hardware can sometimes distinguish more
  than two, but the SDK exposes only `0` and `1`. Design for that.

When a patch has no alternate voicing (`effects/mxr_distortion_plus.cpp`),
either ignore Hold or have it do the same thing as Press. Do not silently
drop Hold without thinking — a player will press-and-hold by accident and
expect *something*.

### LED color conventions

The LED carries voice identity at a glance. Pick a pair of colors per
voice, where the brighter / more saturated color is "active" and the
dimmer is "bypassed." The 16-value enum in
[`source/Patch.h`](../source/Patch.h) is the palette.

Some patterns to copy:

- Drive pedals lean on warm colors: yellows for Klon, greens for Tube
  Screamer, reds for MXR/Big Muff. Bypass goes one shade dimmer.
- Modulation effects lean on cool colors: blues for chorus, magenta/cyan
  for Phase 90.
- Alternate voices change *both* the active and the bypass color (e.g.
  Tube Screamer's TS808 is LightGreen/DimGreen, TS9 is PastelGreen/DarkLime).
  A glance distinguishes active voice on each side of the bypass toggle.

When choosing colors for a new patch, scan
[`effects/README.md`](../effects/README.md) and pick something not yet
taken if you can.

## 3. DSP idioms that work on this hardware

The handful of primitives in [`source/dsp/`](../source/dsp) cover most of
the recurring DSP shapes in the catalogue. Each one was harvested from at
least one effect that used it well — when in doubt, reach for these before
hand-rolling new code.

### One-pole filters

[`source/dsp/filter_coeff.h`](../source/dsp/filter_coeff.h) gives
`dsp::lpCoeff(fc)` and `dsp::hpCoeff(fc)` — the workhorses. Eight of the
fourteen effects use them. A one-pole IIR at 48 kHz single precision is
cheap, well-behaved, and the right answer for the broad voicing filters
most pedal-style effects need before or after a clipper.

The same header also carries `dsp::svfF1(fc, fs)`, the Chamberlin
state-variable coefficient `2*sin(pi*fc/fs)`, used by `wah.cpp`,
`funk_machine_envelope_filter.cpp` and `harmonica.cpp` where a resonant
sweepable filter is the point rather than a fixed voicing shelf. See
[`effects/tube_screamer.cpp`](../effects/tube_screamer.cpp) for the
canonical body/edge split, and
[`effects/klon_centaur.cpp`](../effects/klon_centaur.cpp) for the active
treble shelf.

Recompute the coefficient *only when the cutoff parameter changes*, not per
sample. The recurrence (`state += alpha * (x - state)` for the LP form) is
one multiply and one add per sample; the coefficient computation is two
multiplies and a divide.

That recurrence is exactly what
[`source/dsp/one_pole_filter.h`](../source/dsp/one_pole_filter.h)'s
`dsp::OnePoleLowpass` and `dsp::OnePoleHighpass` classes hold as running
state, alpha passed in per call — the stateful companion to the
coefficient functions above. Use the classes whenever the filter needs to
persist across samples (nearly always); use the bare coefficient functions
when you're computing an alpha to feed into a class you already have.
`big_muff_wdf.cpp` and `tube_screamer_wdf.cpp` both use them for body/edge
splits and tone shaping; `dimension_chorus.cpp` uses them for BBD-style
tap "darkening" and cross-feed shaping.

### Soft-clip safety stage

[`dsp::softLimit`](../source/dsp/soft_limit.h) is the *final* stage before
output for any patch that can excursion past ±1.0. It is a passthrough
inside the linear region (`|x| <= threshold`) and a tanh tail outside,
parameterized by threshold, divisor (knee softness), and tail height
(asymptote offset). The defaults `(0.90, 0.25, 0.10)` give a smooth approach
to ±1.0. Use the parameters when an effect wants a slightly different knee
(`big_muff_wdf` uses `(0.92, 0.24, 0.08)`, `tube_screamer_wdf` uses
`(0.90, 0.22, 0.10)`).

Do not use `softLimit` as the creative non-linearity. The per-effect
character — diode pairs, asymmetric clippers, drive curves — lives where
the effect's voicing decisions are made. `softLimit` is the airbag.

### Aliasing in nonlinear stages

Every clipper, waveshaper, or diode-pair solve in this corpus generates
harmonics. At 48 kHz, any harmonic that lands above 24 kHz doesn't
disappear — it folds back (aliases) into the audible band. None of the
six drive-family effects (`tube_screamer`, `tube_screamer_wdf`,
`klon_centaur`, `big_muff`, `big_muff_wdf`, `mxr_distortion_plus`) do
anything about this today; it is a known, deliberately unaddressed gap,
not an oversight nobody noticed.

Mutable Instruments' own published design history is useful outside
inspiration here (MIT-licensed, similar Cortex-M class hardware): Braids
ran at 96 kHz with naive oversampling to manage this; its successor
Plaits moved to band-limited synthesis "almost everywhere" instead — i.e.
even a well-regarded, shipped product's own design evolved *away* from
naive oversampling as the long-term answer, toward algorithms that don't
generate the offending harmonics in the first place. That's the
North Star if a patch is being designed from scratch around a
band-limitable technique. For an *existing* nonlinearity that isn't
being rewritten, oversampling is the retrofit option — but it is not
free, and this repo has measured, not assumed, what it costs.
[`docs/aliasing-oversampling-experiment.md`](aliasing-oversampling-experiment.md)
2x-oversampled the WDF diode-pair solve in `tube_screamer_wdf.cpp` two
ways (a naive linear-interpolation approach and a proper halfband-FIR
approach) and measured both the alias-energy reduction and the CPU cost
on host. The result: real, measurable alias reduction (more at higher
drive levels, where there's more harmonic content to alias in the first
place), at a real cost — roughly double the wrapped nonlinearity's own
cost, which for an already-nontrivial primitive like the diode solve
(8 transcendental calls per sample) is not a rounding error. The
experiment's own conclusion: document the tradeoff (this section), but
do not apply oversampling to a shipped effect without real Cortex-M7
cycle data justifying the cost — see [`docs/cycle-budget.md`](cycle-budget.md).

### Equal-power crossfade

[`dsp::equalPower(mix)`](../source/dsp/crossfade.h) returns `{dry, wet}`
gains where `dry^2 + wet^2 = 1`. Perceived loudness stays constant across
the sweep. Use for any dry/wet knob; the alternative (linear crossfade) has
a 3 dB dip at `mix=0.5` that reads as "this knob does nothing in the
middle." See `effects/back_talk_reverse_delay.cpp` for the canonical
example.

### LFOs

[`dsp::SineLfo`](../source/dsp/lfo.h) and `dsp::TriangleLfo` both expose a
stateful `tick()` (advance and return the new value) and a stateless
`value(phase)` (compute the wave value for an externally-managed phase).
Choose `tick()` for new effects; the stateless form fits the
`effects/phase_90.cpp` and `effects/chorus.cpp` patterns where the effect
manages phase counters for its own reasons (90° L/R offset, multiple
synchronised LFOs).

A sine LFO costs one `sinf` per sample. A triangle LFO costs an `fabsf` and
a multiply — *much* cheaper. If a patch can tolerate a triangle's harmonic
content, prefer it.

### Fractional-delay reads

[`dsp::lerpRead`](../source/dsp/fractional_delay.h) is the linear-interp
read from a delay buffer at a fractional position. Two memory loads, one
multiply, two adds. Adequate for chorus, vibrato, short delay lines. *Not*
adequate for pitch shifting or long-time-stretch granular — those want a
windowed-sinc interpolator (none provided here; see the `sthompsonjr` fork
for a reference implementation, license permitting).

The `lerpReadPow2` variant uses a bitmask wrap and is the right tool when
the delay length is a power of two — typically when you carved the delay
out of the 2.4M-float working buffer in a power-of-two slice.

### Ring buffer

[`dsp::RingBuffer`](../source/dsp/ring_buffer.h) wraps a slice of the
working buffer with a bitmask-based circular indexer. Use it for delay
lines whose access pattern is "samples back from the write head." For
effects with more exotic access patterns (e.g. `back_talk`'s chunk-based
reverse playback), the raw pointer + manual index calculation is still the
right call — `RingBuffer` is not a universal abstraction.

### Parameter smoother

[`dsp::ParamSmoother`](../source/dsp/parameter_smoother.h) is the
single-pole exponential-approach smoother. Use it on any parameter whose
*coefficient* — clip threshold, biquad coefficient, delay-line read
position — would otherwise glitch when the knob moves. Step changes in
control flow that consumers expect to be continuous are the classic source
of clicks and zipper noise; a 5–20 ms smoother on the knob value removes
them. See `effects/tube_screamer_wdf.cpp` for the canonical pattern.

### DC blocker

[`dsp::DcBlocker`](../source/dsp/dc_blocker.h) is the 1-pole HP that scrubs
DC offset post-non-linearity. Asymmetric clippers, half-wave rectifiers,
and slew limiters all leave DC on their output. Run it as the *last* stage
before the safety `softLimit`. See `effects/harmonica.cpp` for the only
current corpus use.

### Test every primitive on the desktop before it ever meets hardware

Every header above has a matching `tests/dsp/<name>_test.cpp` — endpoint
values, sweeps, edge cases, `reset()` reproducibility — built and run by
`tests/check_dsp.sh` before anything gets near the pedal. This isn't a
process invented for this repo: Mutable Instruments' Braids oscillator
code builds and runs standalone on a desktop
(`make -f braids/test/makefile && ./oscillator_test`), decoupling
algorithm iteration from flash/hardware cycles the same way this corpus's
`tests/dsp/` and `tests/effect_probe.cpp` do. Finding the same convention
independently adopted in a well-regarded, MIT-licensed, similar-Cortex-M-
class codebase is a useful outside check that the pattern is sound, not
just local habit. When adding a new primitive to `source/dsp/`, write its
test alongside it — see any existing `tests/dsp/*_test.cpp` for the shape.

## 4. Working-buffer use patterns

The 9.6 MB working buffer (`Patch::kWorkingBufferSize` floats) is handed to
the patch via `setWorkingBuffer`. It is the only large storage the patch
gets. Spend it carefully.

**Use it for:** delay lines (chorus, reverse delay, modulated delay,
reverb tails when we add one).

**Do not use it for:** lookup tables (put them in `.rodata` via `static
constexpr` arrays), scratch state (put it in patch members), per-sample
temporaries (use locals).

Of the fourteen effects in the catalogue today, only five actually allocate
inside the working buffer:

- `back_talk_reverse_delay`: 2 × 131072 floats (2.73 s per channel)
- `bbe_sonic_stomp`: 2 × 1.5k floats (stereo doubler)
- `chorus`: 2 × 2400 floats (stereo modulated delay)
- `dimension_chorus`: 1 × 2048 floats (single mono ring buffer, not a
  per-channel pair — see section 3's ring buffer subsection)
- `harmonica`: 2 × 480 floats (micro-chorus)

The other nine hold scalar state in members and return early from
`setWorkingBuffer`. That is fine and expected — distortion-family effects
do not need long memory.

When you do carve up the buffer, **prefer power-of-two slice lengths** so
the wrap can be a bitmask AND, not a modulo. The harvest size that survives
the constraint is `kWorkingBufferSize / 2 = 1,200,000` per channel maximum,
which gives ~25 seconds of stereo delay at 48 kHz — plenty.

## 5. State management

Three rules:

1. **`init()` resets everything to the defaults the patch would have at
   power-on.** It is called once on load. Set parameter members to nominal
   values, zero filter state, reset LFO phases, zero ring buffers if the
   working buffer is already attached. Do nothing expensive — `init()`
   should complete in well under a millisecond.
2. **`clearState()` zeros every piece of audio-rate state.** Effects in
   this repo standardise on a private `clearState()` method called from
   `init()` and from the press-action handler when going from bypassed to
   active. Why: a player who has been bypassing for a while expects no
   audible "tail" from the previous active state when they unbypass.
3. **Guard against NaN and infinity at module boundaries.** A single NaN
   propagating through a delay line poisons the patch until `clearState()`
   runs. `dsp::softLimit` happens to filter NaN to NaN, not infinity to a
   bounded value — if a patch has any path that can produce a NaN under
   reasonable input, sanitise *before* the working-buffer write. The
   probe harness (`tests/analyze_effects.sh`) catches this kind of
   regression.

Denormals are not a problem at single precision on Cortex-M7 with the
current toolchain flags (`-fsingle-precision-constant` + the FPU FTZ/DAZ
defaults). Do not chase denormals defensively; if a probe reveals
denormal-related anomalies, fix the algorithm.

## 6. CPU-budget intuition

The repo has no measured cycle data on real hardware yet (see
[`docs/cycle-budget.md`](cycle-budget.md)). Treat 15k cycles/sample as a
working ceiling — that is the figure the `sthompsonjr` fork reports.

A rough mental model of cost on Cortex-M7 single precision:

| Operation | Order of magnitude |
| --- | --- |
| Add / multiply / FMA | ~1 cycle |
| Compare / branch / cmov | ~1 cycle |
| Divide | ~10–15 cycles |
| `sinf`, `cosf`, `tanhf` | ~50–100 cycles |
| `powf`, `expf`, `logf` | ~80–150 cycles |
| L1 hit memory load | ~3 cycles |
| Cache-miss load (delay buffer past a few kB) | ~30+ cycles |

### What the platform's own patches do about transcendentals

Binary analysis of all 42 compiled patches in this repo
([`docs/endl-corpus-study.md`](endl-corpus-study.md), Finding 1) turned up a
clean split: **no Polyend factory plate and no Playground-generated patch
contains a newlib transcendental at all.** All 14 of ours do — 69.3% of our
combined image bytes are newlib libm, up to 83.4% for `wah.cpp`, which is 1,126
bytes of wah wrapped in 6,062 bytes of maths library.

The image size does not matter; every effect sits at 1–3% of the 512 KB region.
What it points at is the table above. `sinf`/`cosf` route through newlib's
`__ieee754_rem_pio2f` argument reduction and `powf` through `__ieee754_powf` —
the branch-heavy, hundreds-of-cycles end of the cost model, not the ~50-cycle
end. Every other patch on the platform appears to avoid that path entirely.

**No cycles have been measured**, here or anywhere in this repo, so this is a
prior about platform norms rather than a demonstrated regression. It is also
not established what those patches do instead; polynomial approximation, table
lookup, or DSP that simply needs no transcendentals all fit the evidence
equally, and their FPU instruction density sits in the same range as ours.

What they do instead, recovered from the disassembly
([`docs/reverse-engineering/factory-patch-idioms.md`](reverse-engineering/factory-patch-idioms.md)):
a cascaded `x / (1 + |x|)` soft-clip — `vabs`, `vadd`, `vdiv`, no call — plus
branchless `ite`-predicated asymmetry and single-instruction `vmaxnm`/`vminnm`
clamping. That doc also measures a clamp idiom worth knowing about: under this
Makefile's `-fno-builtin`, our `if`-chain `clamp01` is 6 instructions and a
branch, plain `fminf`/`fmaxf` become real library calls, and only
`__builtin_fminf`/`__builtin_fmaxf` give the 2-instruction branchless form.

Practical reading: treat a newlib transcendental in a per-sample path as a cost
to justify rather than a default. At control rate it is a non-issue — compute it
once per block, as below.

A few practical consequences:

- **Cache the expensive bits outside the per-sample loop.** Filter
  coefficients, smoother targets, mix gains — compute them once per block
  and reuse. Every effect in this repo does this; the per-block setup
  block is the obvious place.
- **Per-sample `tanhf` is fine if you call it once.** `dsp::softLimit`
  amortises by skipping the call inside the linear region. Two `tanhf`s
  per sample (a creative clipper + a safety stage) is the practical
  ceiling for a single-effect patch.
- **Per-sample `powf` or `expf` is almost never OK.** Move them outside
  the loop. `effects/back_talk_reverse_delay.cpp`'s feedback gain
  computation is the right pattern — `powf` once per block.
  `dimension_chorus.cpp`'s Rate-to-Hz log taper is a real example of
  catching this during review: an early version called `powf` every
  sample to keep pace with a per-sample-stepped smoother, then moved the
  `powf` itself to once-per-block (sampling the smoother's current value
  before the loop) while leaving the smoother stepping every sample —
  the smoothing stays correctly timed, the expensive call doesn't.
  `effects/funk_machine_envelope_filter.cpp` is the one deliberate
  exception to "once per block" in this corpus: its cutoff tracks a
  continuously-varying audio-rate envelope, not a user knob, and the SDK
  doesn't document or guarantee a block size to patches, so it recomputes
  at a self-controlled fixed interval (every 8 samples) instead — see
  that patch's own walkthrough doc (Decision 5) for the full reasoning
  before reaching for the same pattern elsewhere.
  **Not every per-sample recompute of a `dsp::ParamSmoother`-derived value
  is safe to hoist, even when it looks like the same shape as the
  `dimension_chorus.cpp` fix above — measured, not assumed.**
  `tube_screamer_wdf.cpp`/`big_muff_wdf.cpp` compute `cosf`/`sinf` of
  their smoothed `tone_`/`blend_` value every sample; hoisting that to
  block-rate (sampling `.current()` once before the loop, exactly the
  `dimension_chorus.cpp` pattern) was tried as a throwaway experiment: a
  hard knob jump (0.0 → 1.0) followed by ~107 ms of audio showed a
  correlation of 0.999 overall, but the RMS difference between the
  per-sample and hoisted versions was ~29x larger in the first ~10 ms
  after the jump (0.0135) than in the settled tail (0.00046) — a real,
  measured difference concentrated exactly where a knob-move zipper would
  be audible, not an indistinguishable rounding artifact. Left as-is:
  these two effects' per-sample trig is earning its keep, not wasted
  cycles. The difference from `dimension_chorus.cpp`'s case: that fix
  hoists a *rate* knob feeding an LFO, where a block-granularity step is
  perceptually forgiving; `tone_`/`blend_` here feed a crossfade weight
  the ear tracks more closely. Don't assume the two cases generalize to
  each other — re-measure per effect.
- **Long delay-line reads are latency-dominated, not arithmetic — and the
  working buffer is very likely external RAM, not on-chip SRAM.** The
  working buffer (`Patch::kWorkingBufferSize` = 2,400,000 floats, 9.6 MB)
  is where any delay line long enough to matter lives. That capacity
  doesn't fit in on-chip SRAM alongside the ~512 KB patch-image region
  (`internal/patch_imx.ld`'s single `RAM` region) on any Cortex-M7
  variant — the linker-script math only works if the working buffer is
  backed by external SDRAM/PSRAM through the M7's FMC controller. That
  makes a 100 ms delay's read (~4800 samples from the write head) a trip
  off-chip, not just a same-chip cache miss: external-RAM access latency
  is a materially bigger cliff than an on-chip cache miss, even before
  accounting for the FMC's own row-open/refresh overhead on a cold
  access. This is inference from the linker script and Cortex-M7 SRAM
  capacities, not a measurement — no cycle data exists yet (see
  `docs/cycle-budget.md`) — but it sharpens "plan delay-heavy patches
  around the cache" into "budget real headroom for an off-chip round
  trip": do not feed three independent long delay lines from one block,
  and treat a long delay line's read as the most expensive individual
  operation in a patch, not an incidental one.

## 7. Pitfalls

The repo's toolchain flags (`-fsingle-precision-constant`,
`-Wdouble-promotion`, `-fno-exceptions`, `-fno-rtti`, no heap) catch most
of the worst pitfalls at compile time. The rest are:

- **Double-promotion sneaks in via library functions.** `std::pow(x, 2)`
  takes doubles. Use `powf(x, 2.0f)` or `x * x`. The `-Wdouble-promotion`
  warning catches it.
- **Output overflow has no safety net.** The SDK does not clip your
  output for you. Patches in this repo all run their output through
  `dsp::softLimit` or `clampUnit` as the last stage. Forget either and
  you will hear (or worse, leave) a digital crackle.
- **`setParamValue` runs on the audio thread.** It is fine to read those
  values in the loop, but do not do anything in `setParamValue` that
  takes more than a few cycles — no allocation, no syscall, no math more
  expensive than caching the new target into a member.
- **`Patch::getInstance()` is a singleton.** Two patch instances would
  share the singleton; this matters mainly for the
  [`vst/`](../vst) desktop audition build. Inside a real Endless run,
  there is only ever one patch loaded, so it is fine.
- **Initialisation order between `init()` and `setWorkingBuffer()` is
  not guaranteed.** Always sanity-check `delayL_ != nullptr` before
  touching the buffer in `processAudio`. The convention in this repo is
  to early-return from `processAudio` if the working buffer was never
  attached.

## 8. Why this corpus, why not Playground

The Polyend Playground generates patches from a prompt, charging tokens.
That flow has its place. It is also genuinely worse than handcrafted code
at the hardware scale of the Endless, for reasons that get clearer the
more patches you write.

The economics make the case concretely, not just qualitatively: the
Endless ships with 2000 Playground tokens (≈$20) bundled. A simple delay
generation runs roughly $1–2 in tokens; a complex granular looper runs up
to ~$5. Hand-coding via this SDK costs nothing per iteration and has no
ceiling on how many times a control law can be nudged and re-probed
before it's right — the entire fourteen-effect corpus this document
describes, plus every experiment and refactor recorded in
`docs/fork-comparisons/`, cost zero incremental tokens. For
anyone iterating heavily on a control law (which §2 above argues is where
the real craft lives), hand-coding is not just more controllable, it's
the economically rational choice.

A handcrafted Endless patch makes deliberate decisions in places where
prompt-and-pray cannot:

- **Control-law geometry.** §2 above is twenty paragraphs of "where each
  knob's law should live and why." A generator that has not internalised
  those choices will produce knobs that feel arbitrary — fine in the
  middle, weird at the edges, no shared idiom across patches. Players who
  collect several generated patches end up with a control surface that
  fights them. A handcrafted catalogue has a *shared* surface: every drive
  pedal in `effects/` puts Drive on the left, Tone in the middle, Level
  on the right, expression on Right. Picking up a new one takes seconds.
- **Voicing across the knob range.** Sweeping the Drive knob on a good
  overdrive pedal should sound like a continuum, not a step function from
  "almost clean" to "fizzy." That continuity comes from coordinated
  choices across drive curves, filter cutoffs, and mix laws. It is the
  hardest thing for a generator to learn; it is exactly what circuit
  references and §3 above teach.
- **Headroom and safety.** Every effect here lives well inside ±1.0,
  shows it in the probe, and ends with a `softLimit` or `clampUnit`. The
  guarantee is structural: any patch in this repo, fed any reasonable
  guitar signal, will not blow up the firmware's mixer. Generated patches
  routinely lack that guarantee.
- **Reproducibility.** A patch in this repo is a `.cpp` file plus a
  walkthrough plus a circuit reference plus a probe metric. Re-run the
  probe, get the same numbers. Re-flash the hardware, get the same
  voice. A generated patch is a frozen artefact whose decisions are not
  inspectable.

None of this is anti-AI. It is anti-*prompt-and-pray*. AI-assisted
authoring (Claude Code or otherwise) plus this corpus as a reference
produces patches with all four of the above. AI-assisted *re-generation*
from scratch tends not to.

The mission of this fork is to provide that corpus. Every primitive in
[`source/dsp/`](../source/dsp), every effect in [`effects/`](../effects),
and every walkthrough in [`docs/`](.) is here so a future hand-authored
patch can lean on real prior decisions instead of inventing them.
