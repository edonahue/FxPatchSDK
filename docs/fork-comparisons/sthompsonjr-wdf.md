# `sthompsonjr/Endless-FxPatchSDK` WDF Comparison

**Last surveyed:** 2026-04-27 — see [Changes In The Fork Since Last Investigation](#changes-in-the-fork-since-last-investigation)
for the most recent delta.

This note evaluates whether the `sthompsonjr/Endless-FxPatchSDK` fork should change how
this repo authors Polyend Endless effects. The focus is practical: which ideas are worth
stealing, which API divergences should stay out of this repo for now, and where WDF
(Wave Digital Filter) modeling is likely to help more than it hurts.

## TL;DR

In the `sthompsonjr` fork, "WDF" means Wave Digital Filter, not Windows Driver
Framework. That fork adds a serious reusable `wdf/` circuit-modeling layer and a broad
header-only `dsp/` layer that directly overlap with several of this repo's pedal-style
patches. The immediate win is not a wholesale port: it is adopting the primitive-reuse
discipline, documenting WDF as an optional higher-fidelity path, and running one
CPU-budgeted experiment on a patch with a direct analog counterpart before deciding
whether deeper adoption is justified on Endless hardware. That experiment has now begun in
tree via [`effects/big_muff_wdf.cpp`](../../effects/big_muff_wdf.cpp) and
[`effects/tube_screamer_wdf.cpp`](../../effects/tube_screamer_wdf.cpp): two sibling
patches that preserve this repo's control-surface lessons while testing WDF-style
nonlinear stages locally.

## What The Fork Actually Is

`sthompsonjr/Endless-FxPatchSDK` is another fork of
[`polyend/FxPatchSDK`](https://github.com/polyend/FxPatchSDK). It is still an Endless
SDK repo, but it is substantially broader than this repo in two areas:

- `dsp/`: reusable filters, delays, interpolation, modulation, smoothing, saturation,
  pitch, and reverb primitives.
- `wdf/`: reusable Wave Digital Filter elements, semiconductor families, op-amp models,
  and named circuit assemblies.

The fork's generated inventories
([`INVENTORY.md`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/INVENTORY.md),
[`library_inventory.txt`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/library_inventory.txt))
plus a tree walk on 2026-04-27 report:

| Area | Reported scope (2026-04-27) | Why it matters here |
| --- | --- | --- |
| `dsp/` | 23 primitives (was 22 at last survey) | overlaps with our repeated one-pole, mix-law, taper, and smoothing helpers; new entry is `DmmCompander.h` (NE570 model) |
| `wdf/` | 31 files (was 29), including named pedal circuits | directly relevant to our Big Muff, Tube Screamer, Klon, Wah, and Dist+/DOD/RAT-adjacent work; the fork has begun a Deluxe Memory Man family (`DmmCircuits.h`, `DmmBbdCore.h`, `DmmFeedbackLoop.h`, `DmmDelayCircuit.h`) and a five-variant Big Muff stack (`WdfOpAmpBigMuffCircuit.h`, `WdfBigMuffToneStack.h`) |
| `effects/` | 11 completed patches (was 9) | shows how the fork author composes those primitives into real Endless patches; new patches are `PatchImpl_DeluxeMemoryMan.cpp` and `PatchImpl_PowerPuff.cpp` plus a `PowerPuffParams.h` header |
| `tests/` | 23 native test harnesses (was 20) | useful precedent for isolated primitive validation before on-device listening; new tests cover the op-amp Big Muff, the variant-aware tone stack, and the PowerPuff selector |

Those counts are useful orientation, but they are self-reported from generated inventory
files, not a stable API contract. Treat them as a map, not a spec.

The most locally relevant external files are:

- [`dsp/BiquadFilter.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/BiquadFilter.h)
- [`dsp/ParameterSmoother.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/ParameterSmoother.h)
- [`dsp/DmmCompander.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/DmmCompander.h)
- [`wdf/WdfBigMuffCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffCircuit.h)
- [`wdf/WdfOpAmpBigMuffCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfOpAmpBigMuffCircuit.h)
- [`wdf/WdfBigMuffToneStack.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffToneStack.h)
- [`wdf/WdfTubescreamerCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfTubescreamerCircuit.h)
- [`wdf/WdfWahCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfWahCircuit.h)
- [`wdf/DmmBbdCore.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmBbdCore.h)
- [`wdf/DmmCircuits.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmCircuits.h)
- [`wdf/DmmFeedbackLoop.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmFeedbackLoop.h)
- [`wdf/DmmDelayCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmDelayCircuit.h)
- [`effects/PatchImpl_DeluxeMemoryMan.cpp`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PatchImpl_DeluxeMemoryMan.cpp)
- [`effects/PatchImpl_PowerPuff.cpp`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PatchImpl_PowerPuff.cpp)
- [`sdk/Patch.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/Patch.h)
- [`sdk/PatchCppWrapper.cpp`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/PatchCppWrapper.cpp)
- [`CLAUDE.md`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/CLAUDE.md) — fork's documentation-first workflow note (added 2026-04-20)

One important divergence needs to be explicit up front:

- In this repo, [`source/Patch.h`](../../source/Patch.h) has no `isParamEnabled(...)`
  hook, and [`internal/PatchCppWrapper.cpp`](../../internal/PatchCppWrapper.cpp)
  hard-wires expression to param `2`.
- In the `sthompsonjr` fork, `sdk/Patch.h` adds `ParamSource` plus a pure virtual
  `isParamEnabled(int, ParamSource)`, and `sdk/PatchCppWrapper.cpp` delegates routing to
  the patch.

That is a real SDK/API divergence. It is worth analyzing, but this change does not adopt
it.

## Changes In The Fork Since Last Investigation

This section is a delta only. The earlier survey (captured by the rest of this document)
remains the baseline; what follows is what landed on the fork's `master` between then and
the 2026-04-27 walk.

### New Deluxe Memory Man family (BBD delay + compander)

The biggest new direction is the fork's first time-based, non-distortion modeling effort:
a multi-session build of an EH-7850 BBD delay/chorus. The work spans both the WDF and DSP
layers and arrives with its own patch:

- [`wdf/DmmCircuits.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmCircuits.h) —
  EH-7850 signal-conditioning blocks (input/output filtering, chorus mixer)
- [`wdf/DmmBbdCore.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmBbdCore.h) —
  `BBDLine` + `AnalogLfo` wrapper that implements the BBD core
- [`wdf/DmmFeedbackLoop.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmFeedbackLoop.h) —
  the BBD's feedback path with bounded gain
- [`wdf/DmmDelayCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmDelayCircuit.h) —
  IIR-biquad feedback EQ plus the assembled delay topology
- [`dsp/DmmCompander.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/DmmCompander.h) —
  NE570 behavioral compander model, the first dedicated compander primitive in the fork's
  `dsp/` layer
- [`effects/PatchImpl_DeluxeMemoryMan.cpp`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PatchImpl_DeluxeMemoryMan.cpp) —
  the actual patch wiring it all together

For this repo, the practical takeaway is narrower than for the Big Muff family. None of our
current local patches target a BBD or NE570 compander. But the DMM build is the fork's
first non-distortion, non-resonant pedal model, and it doubles as a worked example of how
the fork composes `BBDLine` + `AnalogLfo` + biquad feedback EQ inside one patch — exactly
the question that hangs over our `effects/back_talk_reverse_delay.cpp` and
`effects/chorus.cpp` patches when we eventually decide whether to lean on the fork's BBD
primitives.

### Big Muff family expanded to five variants

The fork's Big Muff coverage went from "three transistor variants" to a much broader stack:

- [`wdf/WdfOpAmpBigMuffCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfOpAmpBigMuffCircuit.h) —
  the EHX V4 op-amp Big Muff (two-stage inverting op-amp with clipping)
- [`wdf/WdfBigMuffToneStack.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffToneStack.h) —
  a variant-aware biquad tone stack supporting all five variants
- a NYC 2N5088 V9 variant added to the existing transistor circuit
- [`effects/PatchImpl_PowerPuff.cpp`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PatchImpl_PowerPuff.cpp) +
  [`effects/PowerPuffParams.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PowerPuffParams.h) —
  one patch that selects between Triangle, Rams Head, Op-Amp, Civil War, and NYC

This is the most directly relevant new work for our [`effects/big_muff.cpp`](../../effects/big_muff.cpp)
and [`effects/big_muff_wdf.cpp`](../../effects/big_muff_wdf.cpp) patches. The interesting
question for us is no longer "is WDF Big Muff worth it?" — it is "is a five-variant
selector with one shared tone-stack object the right way to surface variant choice on
Endless, or does it cost too much to keep that many parallel models alive?" The fork's
PowerPuff patch is the cleanest external precedent for that decision.

### Documentation-first workflow note (`CLAUDE.md`, `build_pipeline.txt`)

On 2026-04-20 the fork added a top-level [`CLAUDE.md`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/CLAUDE.md)
codifying a documentation-first strategy: read `build_pipeline.txt`,
`library_inventory.txt`, and `INVENTORY.md` before touching headers, and regenerate those
docs after each build session. It also fixes hard constraints (no heap / exceptions / RTTI,
single-precision floats, C++20 noexcept on audio paths, 15k cycles/sample budget,
position-independent at `0x80000000`) and naming conventions (`PascalCase.h`, `camelCase`
methods, `snake_case_test.cpp`).

This is a workflow choice, not an SDK change. We do not need to mirror it. It is worth
naming here because it explains why the inventory files in the fork now read more like a
contract — they are intentionally the entry point, not generated artifacts to skim. When
this doc references those inventories, treat them as a stable surface that the fork
maintainer keeps current on purpose.

### Test surface grew with the variant work

Three new native test harnesses landed alongside the variant work:

- `tests/opamp_big_muff_test.cpp` (op-amp Big Muff)
- `tests/tone_stack_test.cpp` (variant-aware tone stack)
- `tests/power_puff_test.cpp` (the five-variant selector)

That brings the fork's native test count from 20 to 23. For our purposes the relevance is
indirect: the fork is still the precedent that says "isolated, headless C++ tests for each
primitive are practical on this codebase" — useful when we decide what to gate any local
shared-DSP extraction on.

### Still no LICENSE file

A re-walk of the fork's tree on 2026-04-27 still does not surface a root `LICENSE` file.
The license open question therefore remains exactly where it was: it has to be answered
before any code (rather than ideas) can be ported.

## Side-By-Side Patch Inventory

The table below maps this repo's current top-level hand-written effects to the closest
asset in the `sthompsonjr` fork.

| Local patch | Current local approach | Closest `sthompsonjr` asset | Likely fidelity delta |
| --- | --- | --- | --- |
| [`effects/back_talk_reverse_delay.cpp`](../../effects/back_talk_reverse_delay.cpp) | reverse-chunk delay, bounded feedback, equal-power mix | `dsp/CircularBuffer.h`, `dsp/MultiTapDelay.h`, `dsp/Interpolation.h`, and now the BBD-style `dsp/BBDLine.h` plus the fork's new `wdf/DmmBbdCore.h` / `dsp/DmmCompander.h` (NE570) | DSP reuse only. WDF still isn't the main value, but the fork's DMM line shows what a BBD-flavored time-based patch looks like end-to-end. |
| [`effects/bbe_sonic_stomp.cpp`](../../effects/bbe_sonic_stomp.cpp) | split-band enhancer, phase/weight compensation, optional doubler | `dsp/BiquadFilter.h`, `dsp/OnePoleFilter.h`, `dsp/HaasStereoWidener.h` | Moderate DSP reuse potential, low direct WDF relevance. |
| [`effects/big_muff.cpp`](../../effects/big_muff.cpp) | cascaded one-pole filters, staged `tanhf`, equal-power dry/wet blend | `wdf/WdfBigMuffCircuit.h` plus the new `wdf/WdfOpAmpBigMuffCircuit.h` (V4 op-amp), `wdf/WdfBigMuffToneStack.h` (variant-aware biquad), and the multi-variant selector pattern in `effects/PatchImpl_PowerPuff.cpp` (Triangle, Rams Head, Op-Amp, Civil War, NYC) | Highest direct-fidelity opportunity in the repo. The fork now exposes five named transistor/op-amp variants behind one selector — useful as a reference for how variant choice could route in our `Mode` story. |
| [`effects/big_muff_wdf.cpp`](../../effects/big_muff_wdf.cpp) | hybrid WDF-style sibling: wave-solved diode pairs plus repo-native `Sustain` / `Tone` / `Blend` UX | `wdf/WdfBigMuffCircuit.h` | This is the in-repo experiment. The remaining question is not "can it be done?" but whether the audible win justifies the higher per-sample cost and extra code. |
| [`effects/chorus.cpp`](../../effects/chorus.cpp) | modulated delay-line chorus | `dsp/BBDLine.h`, `dsp/AnalogLfo.h`, `dsp/WindowedSincInterpolator.h` | High DSP reuse potential, but WDF is not the right abstraction. |
| [`effects/harmonica.cpp`](../../effects/harmonica.cpp) | voiced filter cascade, asymmetric clipping, formant sweep, tremolo | `dsp/StateVariableFilter.h`, `dsp/EnvelopeFollower.h`, `wdf/WdfPnpCircuits.h` | No direct patch counterpart. Selective primitive borrowing may help; full WDF port is not an obvious win. |
| [`effects/klon_centaur.cpp`](../../effects/klon_centaur.cpp) | clean/dirty parallel sum, soft clipping, tone shelf, output stage | `wdf/WdfTubescreamerCircuit.h` (`KlonClipStage`) | Strong nonlinear reference for the clip stage, but not a full drop-in replacement for the patch's current UI and gain structure. |
| [`effects/mxr_distortion_plus.cpp`](../../effects/mxr_distortion_plus.cpp) | hand-tuned drive, tone, level, soft limiter | `wdf/DOD250Circuit.h`, `wdf/WdfRatCircuit.h` | Same family of op-amp/diode distortion problems, but not an exact schematic match. Medium-value research target. |
| [`effects/phase_90.cpp`](../../effects/phase_90.cpp) | hand-authored phaser network, LFO sweep, mode voicing | `dsp/StateVariableFilter.h`, `dsp/AnalogLfo.h`, `dsp/UniVibeLfo.h` | DSP reuse potential exists; WDF is not obviously the best path for this patch. |
| [`effects/tube_screamer.cpp`](../../effects/tube_screamer.cpp) | split-band drive, one-pole tone shaping, output voicing | `wdf/WdfTubescreamerCircuit.h` | Direct higher-fidelity candidate, especially for clipping and op-amp behavior. |
| [`effects/tube_screamer_wdf.cpp`](../../effects/tube_screamer_wdf.cpp) | WDF-style sibling: wave-solved diode pair plus a repo-local `Drive` / `Tone` / `Level` control story | `wdf/WdfTubescreamerCircuit.h` | Also now implemented locally. The open question is whether its more pedal-like output control and clip behavior are preferable to the original expression-on-tone build. |
| [`effects/wah.cpp`](../../effects/wah.cpp) | hand-tuned swept bandpass with soft nonlinear peak behavior | `wdf/WdfWahCircuit.h` | Direct candidate. The fork models the wah as a circuit family rather than a single hand-fit sweep. |

## Duplication Audit

Running a simple occurrence scan over current `effects/*.cpp` plus
[`source/PatchImpl.cpp`](../../source/PatchImpl.cpp) on 2026-04-19 produced a baseline
of **115 helper occurrences**, not the earlier planning estimate of 140. That matters,
because the case for shared primitives is already strong without inflating the numbers.

| Helper | Count | Representative local refs | What a shared primitive would buy us |
| --- | --- | --- | --- |
| `clamp01` | 32 | `effects/big_muff.cpp:29`, `effects/phase_90.cpp:32`, `effects/harmonica.cpp:98` | one canonical control clamp instead of repeated local definitions |
| `clampUnit` | 26 | `effects/big_muff.cpp:40`, `effects/phase_90.cpp:43`, `source/PatchImpl.cpp:21` | one canonical output clamp / soft guardrail |
| `lpCoeff` | 21 | `effects/big_muff.cpp:56`, `effects/klon_centaur.cpp:68`, `effects/bbe_sonic_stomp.cpp:83` | one shared one-pole coefficient helper with clearer naming and test coverage |
| `hpCoeff` | 10 | `effects/big_muff.cpp:51`, `effects/tube_screamer.cpp:61`, `effects/harmonica.cpp:106` | same as above for HP / DC-block style stages |
| `softLimit` | 10 | `effects/mxr_distortion_plus.cpp:47`, `effects/tube_screamer.cpp:49`, `effects/klon_centaur.cpp:51` | one reusable soft-limiter shape with known gain behavior |
| `equalPowerDry` / `equalPowerWet` | 8 total | `effects/big_muff.cpp:62`, `effects/back_talk_reverse_delay.cpp:72` | one tested equal-power crossfade pair instead of per-patch redefinition |
| `mapSpeedHz` / `mapSweepHz` | 8 total | `effects/phase_90.cpp:60`, `source/PatchImpl.cpp:38` | one standard taper/mapping layer for modulation-rate and sweep transforms |

The practical takeaway is simple: even if this repo never ports a single WDF circuit, it
still wants a small shared DSP layer. The current patches have enough local helper
duplication to justify one.

## Design Patterns Worth Adopting

### 1. Header-only reusable primitives

The fork's [`dsp/BiquadFilter.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/BiquadFilter.h),
[`dsp/OnePoleFilter.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/OnePoleFilter.h),
and [`dsp/StateVariableFilter.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/StateVariableFilter.h)
show a discipline this repo should copy even before touching WDF: write one reusable
primitive, then author patches by composing it instead of re-deriving coefficients in
each file.

### 2. Semiconductor families as parametric types

Files such as
[`wdf/WdfDiodeFamily.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfDiodeFamily.h),
[`wdf/WdfAntiparallelDiodeFamily.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfAntiparallelDiodeFamily.h),
and
[`wdf/WdfNpnBjtFamily.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfNpnBjtFamily.h)
replace generic `tanhf` clipping with device-parameterized nonlinear stages. That is the
clearest path from "good enough overdrive feel" to "this actually behaves like a pedal
family."

### 3. Adaptor-based topology encoding

[`wdf/WdfAdaptors.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfAdaptors.h)
and [`wdf/WdfOnePort.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfOnePort.h)
encode series/parallel circuit structure directly in code. That matters because it makes
the mapping from schematic to software auditable instead of burying it inside hand-fit
coefficients and comments.

### 4. Solver modules separated from effect code

[`wdf/LambertW.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/LambertW.h)
and
[`wdf/NewtonRaphson.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/NewtonRaphson.h)
isolate expensive nonlinear math. That separation is useful even if this repo writes its
own lighter abstractions, because it forces authors to ask "what is the per-sample cost
of this decision?" before the math disappears into a patch body.

### 5. Parameter smoothing as a first-class utility

[`dsp/ParameterSmoother.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/ParameterSmoother.h)
is the smallest immediately reusable idea in the fork. This repo currently solves
parameter behavior ad hoc inside each patch. A standard smoother would reduce zipper
noise and make control-law reviews easier to reason about.

## Creative Mapping: WDF Idioms To Endless Patch Authoring

| Fork idiom | Endless concern in this repo | Concrete local mapping | Main caution |
| --- | --- | --- | --- |
| `ParameterSmoother` per knob | zipper-free knob and expression sweeps | sit between `setParamValue(...)` and the per-sample path in our current `Patch` implementations | costs per-sample cycles if applied indiscriminately |
| circuit objects with explicit state | working-buffer vs. hot-state partitioning | keep hot nonlinear state in members; reserve external working buffer for long delays/tables only | do not move hot per-sample state into external RAM |
| adaptor-based topology | keeping schematic intent visible | useful for patches whose sonic identity is mostly the clipping/tone topology, such as Big Muff and Tube Screamer | overkill for delay, chorus, and broad voicing patches |
| per-source parameter enablement | expression-pedal routing | highlights the limitation in [`internal/PatchCppWrapper.cpp`](../../internal/PatchCppWrapper.cpp), where expression is fixed to param `2` at lines 62-68 | adopting the fork API would be an intentional SDK change, not a small helper extraction |
| named circuit variants | footswitch-hold alternate voices | local hold toggles could switch between component presets or voicing structs instead of entirely separate code paths; the fork's `PatchImpl_PowerPuff.cpp` selecting between five Big Muff variants behind one tone-stack object is the closest external precedent | variant swaps need state-clear or crossfade strategy |
| mode-specific LED output | player feedback on compact Endless UI | maps cleanly to existing `getStateLedColor()` patterns already used across local patches | more modes increase UX burden if the sound delta is subtle |

## Costs And Risks

- **CPU budget is the first gate.** The fork's generated inventories attach cycle-cost
  estimates to many WDF blocks, but those numbers are self-reported and have not been
  measured inside this repo's current build/test harness. Any WDF adoption needs a local
  budget experiment on Endless hardware.
- **Template and flash bloat are real risks.** Header-only parametric code is powerful,
  but a careless extract can inflate compile times and binary size quickly.
- **There is a "too accurate to be useful" trap.** A circuit-faithful model is not
  automatically the best Endless patch if the control law becomes cramped, noisy, or hard
  to play with expression.
- **The SDK divergence is non-trivial.** The fork's `isParamEnabled(...)` hook solves a
  real problem, but importing it would change this repo's `Patch` ABI and patch template
  expectations.
- **A bulk migration would still be wasteful.** This repo now has twelve top-level custom
  effects, and several of them do not want WDF at all. Even with two sibling experiments
  now in tree, a repo-wide migration would still blur where the audible wins came from.
- **License compatibility is unresolved.** During this survey, no root `LICENSE` file was
  confirmed in the `sthompsonjr` fork. That must be clarified before porting code rather
  than ideas.

## Recommendations

### Now

Do the documentation and template work only:

- keep this comparison doc as the decision record
- update the patch walkthrough template so authors must choose a modeling-fidelity level
- add a standalone code skeleton that matches this repo's real patch style
- keep this change free of SDK/API modifications

### Next

In a separate change, stand up a small local shared-DSP layer and move only the helpers
this repo already repeats most often:

- one-pole helpers or a tiny `OnePoleFilter`
- a reusable `ParameterSmoother`
- a canonical soft limiter
- equal-power dry/wet helpers
- common taper/mapping helpers

Gate that extraction with:

- [`tests/check_patches.sh`](../../tests/check_patches.sh)
- [`tests/build_effects.sh`](../../tests/build_effects.sh)
- [`tests/analyze_effects.sh`](../../tests/analyze_effects.sh)

### Experiment

Treat the two current sibling patches as phase one of the pedal-modeling spike, not the
end of the decision:

1. Compare [`effects/big_muff.cpp`](../../effects/big_muff.cpp) against
   [`effects/big_muff_wdf.cpp`](../../effects/big_muff_wdf.cpp), because they share the
   same public controls and differ mainly in the nonlinear core.
2. Compare [`effects/tube_screamer.cpp`](../../effects/tube_screamer.cpp) against
   [`effects/tube_screamer_wdf.cpp`](../../effects/tube_screamer_wdf.cpp), focusing on
   whether the WDF-style sibling's `Level` control and clip texture feel more pedal-like
   than the original expression-on-tone design.
3. Use the existing probe harness plus listening tests before extracting any shared WDF
   infrastructure or patch-local helpers into a repo-wide layer.
4. Expand only if the CPU budget stays healthy on Cortex-M7, the hardware performance
   headroom remains comfortable, and the listening result is clearly better than the
   hand-tuned originals.

### Defer

Defer these until a narrower experiment proves value:

- full op-amp model adoption (`LM741`, `LM308`, `JRC4558`)
- any repo-wide `Patch` API change for per-patch expression routing
- broad WDF conversion of non-pedal or mostly time-based patches

## Open Questions

- What is the license status of the `sthompsonjr` fork, and is code porting legally clean?
  As of 2026-04-27 the fork still has no root `LICENSE` file.
- Would the useful direction of contribution be from that fork into this repo, from this
  repo into that fork, or neither?
- Is `WindowedSincInterpolator` worth its reported cost for
  [`effects/chorus.cpp`](../../effects/chorus.cpp) or
  [`effects/back_talk_reverse_delay.cpp`](../../effects/back_talk_reverse_delay.cpp)?
- Should this repo eventually add a lighter-weight expression-routing declaration
  mechanism instead of copying the fork's `isParamEnabled(...)` API verbatim?
- Does the fork's BBD + NE570 compander stack (`DmmBbdCore.h`, `DmmCompander.h`) belong on
  the long list of reusable primitives we'd consider for our own delay/chorus patches, or
  is its only practical use a faithful Deluxe Memory Man emulation we don't intend to
  build?
- Is the `PatchImpl_PowerPuff.cpp` "five Big Muff variants behind one selector" pattern a
  better answer to our `Mode` story than the per-mode hand-tuning we use today, or does
  the Endless UI not benefit from that level of differentiation?

## References

### Local repo references

- [`docs/circuit-to-patch-conversion.md`](../circuit-to-patch-conversion.md)
- [`docs/repository-review.md`](../repository-review.md)
- [`docs/templates/patch-build-walkthrough.md`](../templates/patch-build-walkthrough.md)
- [`docs/templates/patch-code-skeleton.cpp`](../templates/patch-code-skeleton.cpp)
- [`source/Patch.h`](../../source/Patch.h)
- [`internal/PatchCppWrapper.cpp`](../../internal/PatchCppWrapper.cpp)

### External fork references

- <https://github.com/sthompsonjr/Endless-FxPatchSDK>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/INVENTORY.md>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/library_inventory.txt>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/CLAUDE.md>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/BiquadFilter.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/ParameterSmoother.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/DmmCompander.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfOpAmpBigMuffCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffToneStack.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfTubescreamerCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfWahCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmBbdCore.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmCircuits.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmFeedbackLoop.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/DmmDelayCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PatchImpl_DeluxeMemoryMan.cpp>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PatchImpl_PowerPuff.cpp>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/effects/PowerPuffParams.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/Patch.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/PatchCppWrapper.cpp>
