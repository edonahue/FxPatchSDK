# Custom Effects Deep Dive Audit

**Date:** 2026-04-09  
**Scope:** top-level custom patches in `effects/*.cpp`  
**Primary user listening note:** time/modulation patches are broadly usable already, while the drive family is the problem area. Reported hardware impressions were:

- `back_talk_reverse_delay` and `phase_90` are reasonable first passes
- `big_muff` sounds recognizably Muff-like, but the knobs feel narrow
- `tube_screamer` and `klon_centaur` can read as if they have little or no effect

This report treats that hardware feedback as first-class evidence, then uses repo-local measurements to explain why it is happening and what to fix next.

## Remediation Pass Implemented

The first remediation pass from this audit has now been applied to:

- `effects/tube_screamer.cpp`
- `effects/klon_centaur.cpp`
- `effects/mxr_distortion_plus.cpp`
- `effects/big_muff.cpp`

Post-remediation validation was re-run with the same commands:

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
```

### Before / After Snapshot

Low-input nonlinear-content proxy (`sine residual`) at the default settings:

| Patch | Before at 0.35 input | After at 0.35 input | After at 0.15 input | Result |
|---|---:|---:|---:|---|
| `tube_screamer.cpp` | `0.008` | `0.275` | `0.138` | moved from “filtered clean boost” territory into clearly audible overdrive |
| `klon_centaur.cpp` | `0.008` | `0.153` | `0.050` | clipped branch is now clearly present instead of being buried under clean gain |
| `mxr_distortion_plus.cpp` | `0.037` | `0.227` | `0.098` | main distortion control now produces real saturation at quieter input |
| `big_muff.cpp` | `0.343` | `0.327` | `0.273` | core identity remained strong while the sustain sweep became less front-loaded |

Main-knob low-input residual sweeps (`0.35` input scale):

| Patch | Before | After |
|---|---|---|
| `tube_screamer.cpp` | `[0.000, 0.004, 0.012, 0.023, 0.039]` | `[0.083, 0.188, 0.289, 0.351, 0.385]` |
| `klon_centaur.cpp` | `[0.000, 0.006, 0.023, 0.050, 0.113]` | `[0.008, 0.089, 0.230, 0.367, 0.418]` |
| `mxr_distortion_plus.cpp` | `[0.001, 0.027, 0.048, 0.048, 0.037]` | `[0.022, 0.134, 0.259, 0.319, 0.347]` |
| `big_muff.cpp` | `[0.014, 0.272, 0.336, 0.357, 0.367]` | `[0.084, 0.217, 0.304, 0.343, 0.361]` |

### Current Post-Remediation Status

- `tube_screamer.cpp`: no longer reads as near-bypass in the probe. The next risk to verify on hardware is whether the default `Level` is now a little hotter than ideal.
- `klon_centaur.cpp`: now behaves like a real overdrive/boost blend instead of mostly a loud clean path. The next check is whether the remaining clean content still feels Klon-like instead of simply “more gain.”
- `mxr_distortion_plus.cpp`: now tracks much more like a real distortion sweep. It is no longer the obvious weak member of the drive set.
- `big_muff.cpp`: still has the strongest identity of the drive set, but now reaches that identity with a more even sustain ramp.

### Next Hardware Listening Order

After this code pass, the next useful hands-on listening order is:

1. `tube_screamer.cpp`
2. `klon_centaur.cpp`
3. `mxr_distortion_plus.cpp`
4. `big_muff.cpp`
5. `back_talk_reverse_delay.cpp`
6. `phase_90.cpp`

That order is no longer about “which patches are broken”; it is about which patches are most likely to benefit from patch-specific subjective feedback now that the main drive-family failure has been addressed.

## Validation and Method

Validation commands run for this audit:

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
```

Files added for the audit workflow:

- `tests/effect_probe.cpp`
- `scripts/analyze_effects.py`
- `tests/analyze_effects.sh`

Generated artifacts:

- `build/effect_analysis/summary.json`
- `build/effect_analysis/summary.md`

Host probe method:

1. compile each patch against `tests/effect_probe.cpp`
2. feed a deterministic burst signal and a steady sine
3. measure defaults, bypass, hold-mode delta, and three full knob sweeps
4. run the probe at two input scales:
   - `1.00`: nominal / hotter test level
   - `0.35`: quieter guitar-like level
5. spot-check the four drive patches again at `0.15` input scale

Interpretation of the main metrics:

- `burst delta`: how much the processed burst differs from the dry burst overall
- `sine residual`: harmonic/nonlinear content proxy; for the drive patches this is the most important metric
- `fundamental gain`: how much clean gain remains after fitting the main sine component
- `hold diff`: how different the alternate mode is from the default mode

Important limits of the probe:

- linear filters, enhancers, wahs, and choruses can sound meaningful while still showing very little `sine residual`
- modulation rate changes are under-represented by a one-second RMS-style probe
- `chorus` maps hold to the same bypass toggle as press, so its hold delta is not a true alternate-voice metric

## Baseline Pre-Remediation Findings

### 1. `tube_screamer.cpp` is the highest-priority failure

Status: `Critical`  
Target identity: mid-hump overdrive  
Likely failure mode: the patch behaves more like a boosted, filtered clean tone than an overdrive at realistic guitar input levels.

Evidence:

- At `0.15` input scale, default `sine residual` is `0.001`.
- At `0.15` input scale, sweeping `Drive` all the way to `1.0` only raises `sine residual` to `0.008`.
- Over the same sweep, `fundamental gain` rises from `0.543` to `3.773`.
- At `0.35` input scale, default `sine residual` is still only `0.008`, and the full `Drive` sweep only reaches `0.039`.

Interpretation:

- The patch is definitely changing the signal, but the change is dominated by level and voicing.
- The clipping branch is not speaking hard enough at low-to-moderate input level.
- This lines up with the hardware impression that the patch can feel like “no effect” through a clean amp, even though the math says the output is not bypassed.

Fix class:

- `Topology rebalance`
- Then `default-value retune`

Most likely rework:

- raise effective pre-clip level substantially at low/mid `Drive`
- let the clipped branch dominate sooner
- reduce the amount of clean-ish body surviving around default settings
- rebalance output trim after the clipping stage is made audible

### 2. `klon_centaur.cpp` has the same core problem, but even more clean boost

Status: `Critical`  
Target identity: transparent boost-to-overdrive  
Likely failure mode: the clean branch and output stage dominate so strongly that the patch reads as a loud EQ/boost before it reads as a Klon-family overdrive.

Evidence:

- At `0.15` input scale, default `sine residual` is `0.001`.
- At `0.15` input scale, full `Gain` only reaches `0.019` residual.
- Over that same `Gain` sweep, `fundamental gain` rises from `1.264` to `15.619`.
- At `0.35` input scale, default residual is still only `0.008`; full `Gain` reaches `0.113`, which is better than the Tube Screamer but still too clean for the amount of level increase.

Interpretation:

- The clipped branch exists, but its contribution is buried under clean gain and output scaling.
- The patch preserves the “clean/dirty mix” idea conceptually, but the current balance is too conservative for Endless hardware and a clean-amp listening context.
- The user report of “no effect” is consistent with a patch that mostly gets louder and brighter instead of obviously crossing into overdrive.

Fix class:

- `Topology rebalance`
- `Output gain / compensation retune`
- Then `default-value retune`

Most likely rework:

- cut the clean-path dominance earlier in the `Gain` sweep
- increase clipped-branch contribution more aggressively
- reduce the post-sum output boost so “more gain” sounds like more drive, not just more level

### 3. `big_muff.cpp` is basically working, but the control law needs work

Status: `Important`  
Target identity: scooped sustain fuzz  
Likely failure mode: the core fuzz identity is present, but `Sustain` bunches its useful motion too early and the patch flattens out after that.

Evidence:

- At `0.15` input scale, default `sine residual` is already `0.304`.
- At `0.35` input scale, default residual is `0.343`.
- `Sustain` residual sweep at `0.15` is `[0.003, 0.184, 0.291, 0.332, 0.351]`.
- Nominal-output sweep across `Sustain` is fairly flat after the early jump: `[0.308, 0.441, 0.435, 0.428, 0.420]`.
- `Tone` produces a much larger audible range than `Sustain`, and `Blend` behaves as intended.

Interpretation:

- This matches the hardware note almost exactly.
- The patch is not failing to fuzz. It is failing to distribute that fuzz control across the knob travel in a satisfying way.
- The current defaults are already in the “real Muff” zone; the main issue is that the rest of the sweep does not open up enough.

Fix class:

- `Control-law retune`
- Possibly a small `default-value retune`

Most likely rework:

- stretch the low-to-mid `Sustain` region
- reduce how quickly the second clip stage saturates
- consider restoring a little more apparent loudness movement so higher sustain still feels like forward motion

### 4. `mxr_distortion_plus.cpp` is partially working, but still under-clips at quieter input

Status: `Important`  
Target identity: simple Distortion+-style overdrive/distortion  
Likely failure mode: the patch adds obvious level and density, but the nonlinear part is still too restrained at quieter input, and the `Distortion` sweep peaks too early.

Evidence:

- At `0.15` input scale, default `sine residual` is `0.008`.
- At `0.15` input scale, the `Distortion` sweep only reaches `0.010` residual before falling back to `0.007` at full scale.
- The same sweep moves `fundamental gain` much more than harmonic content: `[0.909, 3.594, 4.316, 3.869, 2.983]`.
- At `0.35` input scale, default residual rises to `0.037`, which is better than TS/Klon, but still lighter than expected.

Interpretation:

- This patch is easier to hear than TS/Klon, but it still behaves more like shaped gain than obvious distortion at conservative input level.
- The main knob is not monotonic enough in its useful nonlinear range.

Fix class:

- `Control-law retune`
- `Clipping-gain retune`

Most likely rework:

- make the `Distortion` sweep continue increasing harmonic content deeper into the knob travel
- check whether the current `tanhf` stage needs more gain or a different pre-filter balance

### 5. `bbe_sonic_stomp.cpp` is subtle by design, not broken

Status: `Important, but after the drives`  
Target identity: enhancer / phase-alignment style processor  
Likely failure mode: this patch may read as too polite or too context-dependent, but that is a different problem from the broken drive feel.

Evidence:

- `sine residual` stays at `0.000`, which is expected for a mostly linear enhancer.
- `hold diff` is real (`0.200` burst diff), so the doubler mode is not imaginary.
- Output span across controls is intentionally small.

Interpretation:

- This patch should not be judged on the same “is it clipping enough?” axis as the drives.
- If it feels underwhelming later, the right fix is likely a stronger contour/process range, not distortion changes.

Fix class:

- `Leave alone for now`
- Revisit only after the drive family is fixed

### 6. `back_talk_reverse_delay.cpp` and `phase_90.cpp` are not the current blockers

Status: `Secondary tuning pass`

`back_talk_reverse_delay.cpp`

- Mix sweep and hold-mode texture both measure clearly.
- The probe sees a meaningful reverse-tail signature and a real hold-mode delta.
- This agrees with the hardware note that the patch is broadly usable now.

Likely next pass:

- tune window-time and feedback feel by ear
- revisit default `Mix` and `Repetitions` only after the drives

`phase_90.cpp`

- Hold-mode difference is measurable.
- The probe is not a great tool for judging phaser rate feel, because RMS-style snapshots understate slow modulation-rate changes.
- Hardware feedback says the patch is already reasonable, so this stays in the secondary queue.

Likely next pass:

- tune sweep center, feedback amount, and speed law by ear

### 7. `chorus.cpp` and `wah.cpp` are low-priority from current evidence

`chorus.cpp`

- The host probe is not very meaningful for this patch beyond confirming that wet/dry processing exists.
- Hold is the same bypass toggle as press, so the large hold delta is not an alternate-voice result.
- No urgent failure is indicated.

`wah.cpp`

- The probe shows the expected strong linear filter behavior and almost no nonlinear residual, which is correct for a wah.
- No urgent problem is indicated from the current evidence.

## Baseline Remediation Order

1. `tube_screamer.cpp`
2. `klon_centaur.cpp`
3. `big_muff.cpp`
4. `mxr_distortion_plus.cpp`
5. `bbe_sonic_stomp.cpp`
6. `back_talk_reverse_delay.cpp`
7. `phase_90.cpp`
8. `chorus.cpp`
9. `wah.cpp`

## Baseline Fix-Class Summary

| Patch | Likely fix class | Why |
|---|---|---|
| `tube_screamer.cpp` | topology rebalance, then default retune | current low-input behavior is mostly boost/EQ, not overdrive |
| `klon_centaur.cpp` | topology rebalance, output compensation retune | clean branch dominates the clipped branch too strongly |
| `big_muff.cpp` | control-law retune | identity is present, but the knob travel bunches early |
| `mxr_distortion_plus.cpp` | control-law retune, clipping-gain retune | nonlinear range peaks too early and stays lighter than expected |
| `bbe_sonic_stomp.cpp` | leave alone for now | not a drive failure; subtle by design |
| `back_talk_reverse_delay.cpp` | default retune only | working concept, secondary feel pass |
| `phase_90.cpp` | default retune only | working concept, secondary feel pass |
| `chorus.cpp` | none yet | no strong failure from current evidence |
| `wah.cpp` | none yet | no strong failure from current evidence |

## Practical Follow-Up

For the next implementation pass, the most useful order is:

1. Rework `tube_screamer.cpp` until low-input guitar-level tests produce clearly audible harmonic growth by mid-knob, not only more level.
2. Rework `klon_centaur.cpp` so the clipped branch becomes obvious earlier while still preserving the Klon family feel.
3. Re-map `big_muff.cpp` `Sustain` so the useful range is spread more evenly across the knob.
4. Revisit `mxr_distortion_plus.cpp` after the first three, using it as the simpler distortion baseline for comparison.

Once those four are moved, collect another round of hardware listening notes patch by patch. At that point the remaining modulation and enhancer patches can be tuned as feel improvements instead of as blockers.

## Second Pass: Level & Intensity Tuning

**Date:** 2026-04-16
**Scope:** every patch in `effects/*.cpp`
**Trigger:** post–first-pass hardware listening revealed two remaining shortcomings the harmonic-content rebalance did not address.

### Listening notes driving this pass

1. The wah is filtered and pleasant but not vocal — it lacks the resonant peak and the "growl" that define a real Crybaby/Vox.
2. The drive/fuzz family (`tube_screamer`, `klon_centaur`, `mxr_distortion_plus`, `big_muff`) reaches commercial-pedal harmonic content by knob mid-travel but never reaches commercial-pedal loudness — `Level`, `Output`, and `Blend` caps are ~2–4 dB below unity into a clean amp.
3. `phase_90`, `chorus`, and `bbe_sonic_stomp` sit too quietly in the mix even at "full" settings.

### Changes by patch

| Patch | Root cause | Fix |
|---|---|---|
| `wah.cpp` | SVF bandpass normalized to unity peak (`2/Q`); defaults Q≈5/3 low; linear mix; no op-amp growl | `bpGain = (2/Q) · 2.8` (≈+9 dB at resonance, Q-independent). Defaults raised Q≈7/4.5. `tanh(x·1.2)/1.2` growl on wet. Equal-power dry/wet. Output soft-clip `tanh(x·0.85)/0.85` |
| `tube_screamer.cpp` | `outputTrim = (0.18 + 1.45·lc)·(0.80 − 0.18·drive)` capped at ~1.01 and min ~0.144 (~17 dB) | `(0.12 + 1.72·lc)·(voice.trim − 0.10·drive)` with `voice.outputTrim` 0.80→0.92 (TS808) / 0.84→0.96 (TS9) — ~24 dB range and full commercial loudness at max |
| `klon_centaur.cpp` | Output law `(0.18 + 1.55·oc)·(0.82 − 0.15·gain)` capped ~1.16/min ~0.148 (~18 dB) | `(0.08 + 1.95·oc)·(voice.trim − 0.10·gain)`, `voice.outputBaseTrim` 0.82→0.96 (stock) / 0.86→1.00 (tone-mod) — ~28 dB range matching the real Centaur's famously wide Output |
| `mxr_distortion_plus.cpp` | `levelCurve · (1 − 0.24·drive)` ≤ 0.76 at max drive | Soften drive compensation: `(1 − 0.08·drive)`. Widen level law to `(0.10 + 1.80·levelCurve)`. Add `tanh(lp · outputGain)` output soft-clip so the top of the knob saturates rather than hard-clipping |
| `big_muff.cpp` | `equalPowerWet = 0.94·sin(…)` + low `normalTrim 0.84 − 0.04·sustain` capped wet at ≈0.75–0.84 | Remove 0.94 factor from `equalPowerWet`; raise `normalTrim` base 0.84→0.98 and `bypassTrim` base 0.98→1.08. The existing tanh voicing stages already protect against excursions |
| `phase_90.cpp` | Block feedback 0.36 and wet mix 0.62 — present but not chewy | Block feedback 0.36→0.58 and wet mix 0.62→0.72; script voice untouched beyond a small wet-mix lift (0.58→0.62) |
| `chorus.cpp` | Depth capped at ±10 ms; linear mix crossfade | Depth range 1–13 ms peak (still safe against 720-sample center); equal-power Mix crossfade |
| `bbe_sonic_stomp.cpp` | Blend caps 0.85/0.75/0.80 — correction barely reaches audible on sustained chords | Raise caps to 1.10/1.00/1.05 (~+2 dB each). Output clamp catches simultaneous-max excursions |
| `back_talk_reverse_delay.cpp` | `equalPowerWet = 0.92·sin(…)` prevents full-wet | Remove 0.92 factor so Mix=1 is truly full-wet |

### Gain-staging template adopted

Across the drive family this pass standardizes on:

```
levelCurve = level · (0.5 + 0.5 · level)              // or level^{1.0–1.1}
outputGain = (base + span · levelCurve) · (voice.trim − dropoff · driveCurve)
```

With the empirical ranges:

- `base` ≈ 0.08–0.12 (minimum knob produces a usable small signal, not silence)
- `span` ≈ 1.70–1.95 (so max knob can push soft-clipped saturation above unity before the limiter)
- `dropoff` ≈ 0.08–0.10 (mild loudness compensation at max drive, not an aggressive pull-down)
- `voice.trim` ≈ 0.92–1.00 (per-voice trim; ≥0.92 so max level reaches commercial loudness)

This replaces the earlier `(0.18 + 1.45·lc)·(0.80 − 0.24·drive)` family, which consistently capped peaks in the 0.76–0.84 range.

### Verification for this pass

1. `bash tests/check_patches.sh` — fast host lint/syntax
2. `bash tests/build_effects.sh` — ARM cross-compile
3. `bash tests/analyze_effects.sh` — confirm:
   - drive family peak ≥ 0.92 at max Level on a sustained sine
   - wah peak ≥ +6 dB over input at center frequency
   - phaser sweep visibly deeper (FFT notches travel further)
4. Hardware listening in order: wah (primary) → drive family → phaser → chorus → enhancer → delay
