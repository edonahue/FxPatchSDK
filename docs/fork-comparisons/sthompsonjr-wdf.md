# `sthompsonjr/Endless-FxPatchSDK` WDF Comparison

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
whether deeper adoption is justified on Endless hardware.

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
report roughly:

| Area | Reported scope | Why it matters here |
| --- | --- | --- |
| `dsp/` | 22 primitives | overlaps with our repeated one-pole, mix-law, taper, and smoothing helpers |
| `wdf/` | 29 files, including named pedal circuits | directly relevant to our Big Muff, Tube Screamer, Klon, Wah, and Dist+/DOD/RAT-adjacent work |
| `effects/` | 9 completed patches | shows how the fork author composes those primitives into real Endless patches |
| `tests/` | 20 native test harnesses | useful precedent for isolated primitive validation before on-device listening |

Those counts are useful orientation, but they are self-reported from generated inventory
files, not a stable API contract. Treat them as a map, not a spec.

The most locally relevant external files are:

- [`dsp/BiquadFilter.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/BiquadFilter.h)
- [`dsp/ParameterSmoother.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/ParameterSmoother.h)
- [`wdf/WdfBigMuffCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffCircuit.h)
- [`wdf/WdfTubescreamerCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfTubescreamerCircuit.h)
- [`wdf/WdfWahCircuit.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfWahCircuit.h)
- [`sdk/Patch.h`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/Patch.h)
- [`sdk/PatchCppWrapper.cpp`](https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/PatchCppWrapper.cpp)

One important divergence needs to be explicit up front:

- In this repo, [`source/Patch.h`](../../source/Patch.h) has no `isParamEnabled(...)`
  hook, and [`internal/PatchCppWrapper.cpp`](../../internal/PatchCppWrapper.cpp)
  hard-wires expression to param `2`.
- In the `sthompsonjr` fork, `sdk/Patch.h` adds `ParamSource` plus a pure virtual
  `isParamEnabled(int, ParamSource)`, and `sdk/PatchCppWrapper.cpp` delegates routing to
  the patch.

That is a real SDK/API divergence. It is worth analyzing, but this change does not adopt
it.

## Side-By-Side Patch Inventory

The table below maps this repo's current top-level hand-written effects to the closest
asset in the `sthompsonjr` fork.

| Local patch | Current local approach | Closest `sthompsonjr` asset | Likely fidelity delta |
| --- | --- | --- | --- |
| [`effects/back_talk_reverse_delay.cpp`](../../effects/back_talk_reverse_delay.cpp) | reverse-chunk delay, bounded feedback, equal-power mix | `dsp/CircularBuffer.h`, `dsp/MultiTapDelay.h`, `dsp/Interpolation.h` | DSP reuse only. WDF is not the main value here. |
| [`effects/bbe_sonic_stomp.cpp`](../../effects/bbe_sonic_stomp.cpp) | split-band enhancer, phase/weight compensation, optional doubler | `dsp/BiquadFilter.h`, `dsp/OnePoleFilter.h`, `dsp/HaasStereoWidener.h` | Moderate DSP reuse potential, low direct WDF relevance. |
| [`effects/big_muff.cpp`](../../effects/big_muff.cpp) | cascaded one-pole filters, staged `tanhf`, equal-power dry/wet blend | `wdf/WdfBigMuffCircuit.h` | Highest direct-fidelity opportunity in the repo. The fork encodes transistor stages, diode pair, and named variants more explicitly than our current hand tuning. |
| [`effects/chorus.cpp`](../../effects/chorus.cpp) | modulated delay-line chorus | `dsp/BBDLine.h`, `dsp/AnalogLfo.h`, `dsp/WindowedSincInterpolator.h` | High DSP reuse potential, but WDF is not the right abstraction. |
| [`effects/harmonica.cpp`](../../effects/harmonica.cpp) | voiced filter cascade, asymmetric clipping, formant sweep, tremolo | `dsp/StateVariableFilter.h`, `dsp/EnvelopeFollower.h`, `wdf/WdfPnpCircuits.h` | No direct patch counterpart. Selective primitive borrowing may help; full WDF port is not an obvious win. |
| [`effects/klon_centaur.cpp`](../../effects/klon_centaur.cpp) | clean/dirty parallel sum, soft clipping, tone shelf, output stage | `wdf/WdfTubescreamerCircuit.h` (`KlonClipStage`) | Strong nonlinear reference for the clip stage, but not a full drop-in replacement for the patch's current UI and gain structure. |
| [`effects/mxr_distortion_plus.cpp`](../../effects/mxr_distortion_plus.cpp) | hand-tuned drive, tone, level, soft limiter | `wdf/DOD250Circuit.h`, `wdf/WdfRatCircuit.h` | Same family of op-amp/diode distortion problems, but not an exact schematic match. Medium-value research target. |
| [`effects/phase_90.cpp`](../../effects/phase_90.cpp) | hand-authored phaser network, LFO sweep, mode voicing | `dsp/StateVariableFilter.h`, `dsp/AnalogLfo.h`, `dsp/UniVibeLfo.h` | DSP reuse potential exists; WDF is not obviously the best path for this patch. |
| [`effects/tube_screamer.cpp`](../../effects/tube_screamer.cpp) | split-band drive, one-pole tone shaping, output voicing | `wdf/WdfTubescreamerCircuit.h` | Direct higher-fidelity candidate, especially for clipping and op-amp behavior. |
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
| named circuit variants | footswitch-hold alternate voices | local hold toggles could switch between component presets or voicing structs instead of entirely separate code paths | variant swaps need state-clear or crossfade strategy |
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
- **A bulk migration would be wasteful.** This repo has several patches where WDF is not
  the dominant need. Porting all ten local effects at once would blur where the actual
  audible wins came from.
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

Run one scoped pedal-modeling spike before committing to broader WDF adoption:

1. Start with [`effects/big_muff.cpp`](../../effects/big_muff.cpp), because it has the
   cleanest direct counterpart and the strongest "pedal identity" upside.
2. Keep the experiment narrow at first: replace or prototype the current clipping stage
   with a device-parameterized diode family or a minimal WDF sub-stage before attempting
   a full named-circuit port.
3. Compare the result with the existing behavior probe plus a listening test.
4. Only expand to [`effects/tube_screamer.cpp`](../../effects/tube_screamer.cpp) if CPU
   headroom remains comfortable and the listening result is clearly better.

### Defer

Defer these until a narrower experiment proves value:

- full op-amp model adoption (`LM741`, `LM308`, `JRC4558`)
- any repo-wide `Patch` API change for per-patch expression routing
- broad WDF conversion of non-pedal or mostly time-based patches

## Open Questions

- What is the license status of the `sthompsonjr` fork, and is code porting legally clean?
- Would the useful direction of contribution be from that fork into this repo, from this
  repo into that fork, or neither?
- Is `WindowedSincInterpolator` worth its reported cost for
  [`effects/chorus.cpp`](../../effects/chorus.cpp) or
  [`effects/back_talk_reverse_delay.cpp`](../../effects/back_talk_reverse_delay.cpp)?
- Should this repo eventually add a lighter-weight expression-routing declaration
  mechanism instead of copying the fork's `isParamEnabled(...)` API verbatim?

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
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/BiquadFilter.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/dsp/ParameterSmoother.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfBigMuffCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfTubescreamerCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/wdf/WdfWahCircuit.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/Patch.h>
- <https://github.com/sthompsonjr/Endless-FxPatchSDK/blob/master/sdk/PatchCppWrapper.cpp>
