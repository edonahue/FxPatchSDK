# Patch Authoring Best Practices for the Polyend Endless

This document is the canonical "how to handcraft a good Endless patch" reference.
It crystallises lessons learned across the twelve effects in
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

When in doubt, **Mix on the Right knob, expression-mapped**. Every effect in
this fork that exposes a dry/wet blend puts it there. This convention is the
reason a player can pick up any of the twelve effects and know within a few
seconds where the wet/dry control lives.

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
twelve effects use them. A one-pole IIR at 48 kHz single precision is
cheap, well-behaved, and the right answer for the broad voicing filters
most pedal-style effects need before or after a clipper. See
[`effects/tube_screamer.cpp`](../effects/tube_screamer.cpp) for the
canonical body/edge split, and
[`effects/klon_centaur.cpp`](../effects/klon_centaur.cpp) for the active
treble shelf.

Recompute the coefficient *only when the cutoff parameter changes*, not per
sample. The recurrence (`state += alpha * (x - state)` for the LP form) is
one multiply and one add per sample; the coefficient computation is two
multiplies and a divide.

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

## 4. Working-buffer use patterns

The 9.6 MB working buffer (`Patch::kWorkingBufferSize` floats) is handed to
the patch via `setWorkingBuffer`. It is the only large storage the patch
gets. Spend it carefully.

**Use it for:** delay lines (chorus, reverse delay, modulated delay,
reverb tails when we add one).

**Do not use it for:** lookup tables (put them in `.rodata` via `static
constexpr` arrays), scratch state (put it in patch members), per-sample
temporaries (use locals).

Of the twelve effects in the catalogue today, only four actually allocate
inside the working buffer:

- `back_talk_reverse_delay`: 2 × 131072 floats (2.73 s per channel)
- `bbe_sonic_stomp`: 2 × 1.5k floats (stereo doubler)
- `chorus`: 2 × 2400 floats (stereo modulated delay)
- `harmonica`: 2 × 480 floats (micro-chorus)

The other eight hold scalar state in members and return early from
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
- **Long delay-line reads are cache-miss-dominated, not arithmetic.** A
  100 ms delay reads a sample that is ~4800 cache lines away from the
  write head. Plan delay-heavy patches around the cache; do not try to
  feed three independent long delay lines from one block.

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
