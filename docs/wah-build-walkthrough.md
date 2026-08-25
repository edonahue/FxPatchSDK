# Wah Wah Patch — Build Walkthrough

**File:** `effects/wah.cpp`  
**Date:** April 2026  
**Filter type:** Chamberlin State-Variable Filter (SVF)  
**Modes:** Dunlop Crybaby GCB-95 · Vox V847  
**Template:** [`docs/templates/patch-build-walkthrough.md`](templates/patch-build-walkthrough.md)

> **2026-06-13 update:** the Mix-knob equal-power crossfade now uses
> `dsp::equalPower` from [`source/dsp/`](../source/dsp); behavior is unchanged.

---

## Overview

A wah pedal is a bandpass filter whose center frequency is swept by the player's foot. The
resonant peak creates a vowel-like formant that rises and falls — "wah." This patch implements
two classic characters: the Dunlop Crybaby (aggressive, nasal, wide sweep) and the Vox V847
(warmer, smoother, slightly narrower).

The expression pedal input drives the sweep. The three knobs control Mix, Q, and the wah
position when no expression pedal is connected (a "parked wah" tone). Hold the footswitch to
toggle between modes.

---

## Circuit Reference

### Dunlop Crybaby GCB-95

The original Crybaby uses an inductor (Fasel or Halo type) and capacitors to form a resonant
LC bandpass with the op-amp stage. The sweep is produced by a potentiometer in the expression
pedal mechanism varying the effective impedance in the filter network.

**Key measured parameters:**
- Heel frequency: ~350 Hz
- Toe frequency: ~2.5 kHz
- Resonance Q: approximately 4–6 at the peak
- Character: pronounced, nasal, aggressive onset; classic rock/funk tone

### Vox V847

The Vox uses a similar inductor-capacitor topology but different component values, particularly
a different inductor and RC network that softens the resonance and reduces the upper sweep
frequency slightly.

**Key measured parameters:**
- Heel frequency: ~350 Hz
- Toe frequency: ~2.2 kHz
- Resonance Q: approximately 2–4 at the peak
- Character: warmer, more vocal, less nasal; popular for jazz, fusion, smooth funk

### DSP simplification

Neither pedal's exact component values are replicated component-by-component (which would
require Wave Digital Filters). Instead, the measured parameters (fc range, Q) are used directly
as DSP targets. This is the "simplified physically-informed model" approach — see
[Yeh & Abel, DAFX 2007](https://ccrma.stanford.edu/~dtyeh/papers/yeh07_dafx_distortion.pdf).

---

## Decision 1 — Filter Topology (superseded 2026-08-24, see addendum)

**Goal:** A resonant bandpass filter with independently controllable center frequency and Q.

**Options considered (at the time):**

| Option | Pros | Cons |
|---|---|---|
| Cascaded 1-pole HP + LP | Very simple code | Cannot produce resonant peak (Q > 0.5); sounds nothing like a wah |
| Biquad bandpass (Audio EQ Cookbook) | Well-documented; standard; 5 coefficients | Coefficient update involves sinf + cosf; Q and fc coupled in some formulations |
| Chamberlin SVF (2nd order) | 3 lines/sample; direct BP output; f1 and q1 independent; same cost | Chamberlin-specific stability constraint (see below) |

**Chose (originally): Chamberlin SVF.** The state-variable filter directly yields bandpass, lowpass, and
highpass outputs from a single pass through three equations. The frequency coefficient `f1`
and damping `q1` are completely independent, which maps naturally to the Q knob and the
expression-pedal-swept frequency.

**This reasoning turned out to be backwards.** The biquad's "con" — "Q and fc coupled in
some formulations" — is a real property of the Chamberlin SVF, not the biquad: measuring
the *actual* resonant peak (not just the pole angle) shows it drifts up to 174 cents from
its target as Q drops, while the RBJ biquad measures 0.007 cents error across the same
sweep. See the 2026-08-24 addendum below — this decision was reversed.

**Stability note (on the now-replaced SVF):** The Chamberlin SVF can become unstable if
`f1 > 2*q1`. Within our parameter space (fc ≤ 2.5 kHz at 48 kHz, Q ≤ 10):
- Maximum f1 = 2×sin(π×2500/48000) ≈ 0.325
- Minimum 2×q1 at Q=10: 2×(1/10) = 0.2

This was flagged as a borderline condition at Q=10 and fc=2500 Hz. In practice it never
actually diverged — direct simulation across the full fc×Q grid never produced a NaN or
runaway state. The real risk at that corner turned out to be the much larger, undetected
detuning error above, not instability.

---

## Decision 2 — Gain Normalization (superseded 2026-08-24, see addendum)

**Problem (as originally understood):** The Chamberlin SVF bandpass output has peak gain =
Q/2. At Q=10, the bandpass signal is 5× the input amplitude at the resonant frequency — much
too loud.

**This premise was wrong.** Direct measurement of the actual recursion
(`low += f1*band; hi = in - low - q1*band; band += f1*hi`) across Q=1..20 shows the true raw
peak gain is **Q**, not Q/2 — confirmed to 4 decimal places. See the addendum for what this
meant in practice (the `(2/Q)` correction below happened to cancel the real Q-dependence
anyway, just not for the reason this section claimed).

**Solution (as shipped, until 2026-08-24):** Multiply the bandpass output by `2.0f * q1` (= 2/Q):

```cpp
const float bpGain = 2.0f * q1;  // 2/Q
// Per sample:
float wetL = bandL_ * bpGain;
```

This was intended to normalize the peak gain to exactly 1.0 (0 dB) at the resonant frequency,
independent of Q — and it *did* end up Q-independent, but at 2× the intended peak (since the
real raw peak is Q, and `(2/Q)*Q = 2`, not 1). Off-resonance, the output is attenuated by the
bandpass rolloff — which is what creates the wah character when blended with the dry signal
via the Mix knob; that part of the reasoning holds regardless of filter form.

**Gain staging result (Mix = 1.0, full wet), as understood at the time:**
- At resonant frequency: output = 1.0 × input (unity gain) — **actually 2× input**, per above
- Well away from resonance: output → 0 (bandpass rejection)
- The wah sweep moves the resonance → creates the classic vowel-filter sweeping effect

---

## Decision 3 — Control Layout (Expression Pedal Constraint)

**Constraint:** The expression pedal is hardcoded to param 2 (Right knob) in
`internal/PatchCppWrapper.cpp`. This is a global SDK setting affecting all patches — not
per-patch configurable in the current SDK version.

**Implication:** The expression pedal calls `setParamValue(2, value)` with heel=0.0, toe=1.0.
This means **param 2 IS the wah sweep position**, and the Right knob acts as a "parked wah"
control when no expression pedal is connected.

**The Sweep Range problem:** The original design brief asked for Mix, Q, and a third
creative knob (Sweep Range was selected). However, with Right/param 2 dedicated to the sweep
position, there is no knob left for a continuous Sweep Range control.

**Resolution:** Sweep Range variation is captured in the mode system:
- Crybaby mode: 350 Hz → 2500 Hz (wide, aggressive)
- Vox mode: 350 Hz → 2200 Hz (narrower, warmer)

This is actually a better design for live performance — the player selects a sweep character
with the footswitch hold rather than adjusting a knob mid-phrase. The final knob layout:

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | Mix | 0=dry, 1=full wet | 1.0 (full wet) |
| Mid | 1 | Q resonance | 0→Q=1.0, 1→Q=10.0 | 0.444 (→ Q≈5) |
| Right / Exp | 2 | Wah position | 0=heel/350Hz, 1=toe/fc_max | 0.5 (mid/"cocked wah") |

**Considered and declined** (see `CLAUDE.md`): a virtual `isParamEnabled()` on `Patch.h` would
allow this patch to declare the expression pedal on a
non-Right knob, freeing param 2 for Sweep Range. Not available in the stock SDK.

---

## Decision 4 — Frequency Taper

**Problem:** A linear frequency sweep sounds uneven to the ear — the wah rushes through the
lower octave quickly and lingers in the upper range.

**Linear:**
```
fc = 350 + wahPos * (fc_max - 350)
```
At wahPos=0.5: fc = 350 + 0.5×2150 = 1425 Hz — feels "already too far up"

**Logarithmic (chosen):**
```
fc = fc_min * powf(fc_max / fc_min, wahPos)
```
At wahPos=0.5: fc = 350 × √(2500/350) ≈ 935 Hz — geometrically centered, feels balanced

The log taper is computed once per buffer (not per sample), so `powf` is called at approximately
48 Hz — negligible CPU cost. The player experiences a smooth, musically even wah sweep from
heel to toe.

---

## Decision 5 — Footswitch UX (Hardware Constraint)

**User expectation:** Use a second footswitch to toggle between Crybaby and Vox modes.

**Hardware reality:** The Endless has two physical footswitches, but the SDK only exposes
ONE switch with two event types in the `ActionId` enum:
- `kLeftFootSwitchPress` (idx=0): short press
- `kLeftFootSwitchHold` (idx=1): held press

The second physical footswitch is firmware-reserved and not accessible to patch code in the
current SDK.

**Resolution:** Map the two events of the single exposed switch:
- **Short press** (idx=0) → bypass toggle (standard pedal behavior; most expected)
- **Hold** (idx=1) → Crybaby ↔ Vox mode toggle (less frequently needed; hold is natural)

On mode toggle, filter state is cleared to prevent a transient pop from the fc jump.

**Future improvement:** If Polyend exposes the second footswitch in a future SDK update,
map `kRightFootSwitchPress` → mode toggle, freeing the hold for a third function (e.g.,
momentary auto-wah or tap-tempo LFO rate).

---

## Decision 6 — LED States

Four distinct states, using verified `Color` enum values from `source/Patch.h`:

| State | Color | `Color` enum |
|---|---|---|
| Crybaby, active | Red | `Color::kRed` |
| Vox, active | Yellow | `Color::kLightYellow` |
| Crybaby, bypassed | Dark red | `Color::kDarkRed` |
| Vox, bypassed | Dim yellow | `Color::kDimYellow` |

**Rationale for dim-mode-color on bypass:** The player can see which mode they will return to
when they re-engage the effect. This is more informative than a generic DimWhite (used by
other patches in this repo) and appropriate here because mode switching requires a deliberate
hold action — you want to know where you left it.

---

## Decision 7 — State Clearing Strategy

| Event | Clear filter state? | Reason |
|---|---|---|
| Bypass → active (press) | **Yes** | Stale lowL_/bandL_ values from before bypass would cause a pop |
| Active → bypass (press) | No | Signal is just silenced; no state read |
| Mode toggle (hold) | **Yes** | fc jumps between modes; old state at wrong fc causes a transient |
| Expression pedal moves | **No** | State continuity creates the smooth sweep character |
| Q knob changes | No | Gradual parameter change; continuity is desirable |
| Mix knob changes | No | No filter state involved |

```cpp
void clearFilterState() {
    lowL_ = bandL_ = lowR_ = bandR_ = 0.0f;
}
```

---

## Implementation Notes

### No working buffer needed

The filter requires only 2 float state variables per channel (4 total: `bpL_`/`bpR_`, each a
`dsp::BandpassBiquad`). These live as class members in internal SRAM. The external working
buffer (2.4M floats) is not used.

### CPU budget

**This section was written for the original Chamberlin SVF and never updated when the growl
and output-limiter tanh stages were added — it significantly understated the real per-sample
cost even before the 2026-08-24 filter swap. Corrected here.**

Per sample cost (one channel):
- Biquad: `dsp::BandpassBiquad::process` — 2 multiplies computing the output, 4 more updating
  state (see `source/dsp/biquad.h`) — comparable to the 3-multiply Chamberlin recursion it
  replaced.
- Growl stage: 1 multiply (gain) + **1 `tanhf` call**.
- Output limiter: 1 multiply (mix) + 1 multiply (drive) + **1 `tanhf` call**.

**Two `tanhf` calls per channel per sample — four per stereo sample pair, unconditional, not
gated by an overshoot check.** This is the dominant per-sample cost in the patch by a wide
margin; the biquad/filter math itself is a handful of multiply-adds. Whether these can be
replaced with something cheaper was investigated directly (not assumed) as part of the
2026-08-24 work — see the addendum below for the outcome.

Coefficient computation (`dsp::rbjBandpassCoeffs`: one `sinf`, one `cosf`, one divide) happens
once per `processAudio` call, not per sample — negligible at typical block sizes, same as the
`powf` log-taper computation above it.

### Parked wah (no expression pedal)

When no expression pedal is connected, the Right knob manually sets `wahPos_`. The default
(0.5) produces a "cocked wah" tone — the filter parked at mid-sweep (~935 Hz). This is a
popular guitar tone in its own right (used in funk and R&B) and makes the patch usable even
without the expression pedal connected.

---

## Testing

```bash
# Syntax and lint check (from repo root)
bash tests/check_patches.sh
```

**Expected:** `PASS: effects/wah.cpp` with 0 lint warnings.

**Manual listening checklist (hardware):**
- [ ] Wah sweep from heel to toe is smooth and even (log taper working)
- [ ] Q knob tightens the resonance from broad to nasal as it increases
- [ ] Mix=0 passes dry signal completely (bypass-like but effect still running)
- [ ] Mix=1 is classic wah — no dry bleed
- [ ] Hold footswitch toggles LED from Red → Yellow (Crybaby → Vox)
- [ ] Vox mode sounds warmer and slightly less extended at toe
- [ ] Short press bypasses; LED dims to DarkRed or DimYellow depending on current mode
- [ ] Re-engaging after bypass has no pop or click
- [ ] Mode toggle while active has no pop or click
- [ ] **New 2026-08-24:** toe position hits the intended bright endpoint at every Q
      setting, not just at high Q — sweep the Q knob at toe and confirm the pitch of
      the resonant peak stays put (Q used to visibly detune it, worst at Q=1)
- [ ] **New 2026-08-24:** loudness at the resonant peak stays roughly constant as the
      Q knob is swept full-range (it was already meant to be constant; confirm it
      still is with the new filter)

---

## Future Improvements

1. **Auto-wah LFO:** When no expression pedal is connected, an LFO could sweep `wahPos_`
   automatically. A Rate knob could replace the fixed-position parked wah behavior.

2. **Per-patch expression pedal routing:** would need a virtual `isParamEnabled()` on `Patch.h` — declined for now, see `CLAUDE.md`.
   This would allow assigning the expression pedal to
   a different param, freeing param 2 (Right knob) for a Sweep Range control.

3. **Envelope follower (auto-wah):** Use the input signal amplitude to drive `wahPos_`
   instead of (or in addition to) the expression pedal. A touch-wah effect.

4. **Second footswitch:** If Polyend exposes `kRightFootSwitchPress` in a future SDK update,
   reassign mode toggle to that switch for more ergonomic control.

---

## 2026-08-24 — fixing the Q-dependent detuning and gain-staging bugs

Applying this repo's reverse-engineering methodology (originally built for
`funk_machine_envelope_filter.cpp`) to `wah.cpp` surfaced two real bugs in the
original Chamberlin SVF implementation, both dating to the initial release.

### Bug 1 — the resonant peak was not where the knobs said it was

Driving the actual per-sample recursion with swept sine tones and measuring where
the output truly peaks — not where the pole angle points, which diverge for this
filter's zero structure at low Q — shows the resonant frequency drifts sharply as
Q drops:

| fc target | Q=1 (old error) | Q=10 (old error) |
|---|---|---|
| 350 Hz (heel) | +20.7 cents | +3.5 cents |
| 935 Hz (mid) | +57.9 cents | +6.9 cents |
| 2500 Hz (toe, Crybaby) | **+174.4 cents** | +13.8 cents |

At Q=1 — a fully reachable knob position — the Crybaby toe position, intended to
hit 2500 Hz, actually peaked at 2765 Hz: 1.7 semitones sharp. Verified two
independent ways: a C++ probe running the exact recursion, and a from-scratch
Python reimplementation, agreeing to the Hz. This is a documented limitation of
the Chamberlin SVF in the DSP literature — Lazzarini & Timoney,
["Improving the Chamberlin Digital State Variable Filter"](https://arxiv.org/abs/2111.05592)
(arXiv:2111.05592) — not something specific to this codebase.

**Fix:** replaced the Chamberlin SVF with the RBJ constant-peak-gain bandpass
biquad (`dsp::rbjBandpassCoeffs` / `dsp::BandpassBiquad`,
[`source/dsp/biquad.h`](../source/dsp/biquad.h)), whose peak sits at exactly the
target frequency for any Q, by construction. Measures **0.007 cents** worst-case
error across the same grid — see
[`tests/wah_svf_accuracy_probe.cpp`](../tests/wah_svf_accuracy_probe.cpp), which
keeps the old recursion around specifically so this comparison stays
reproducible. Reversed Decision 1 above: the biquad's "con" was in fact the
Chamberlin SVF's own, larger problem.

This is not wah-specific — `funk_machine_envelope_filter.cpp` (Q floor 1.2, 165
cents worst case) and `harmonica.cpp` (Q floor 2.0–3.0, 51 cents) were also
measurably affected and received the same fix.

### Bug 2 — the "peak gain = Q/2" premise behind Decision 2 was wrong

Independent of the detuning: direct measurement of the *raw* Chamberlin
recursion's peak amplitude (before any correction) across Q=1..20 shows it is
**Q**, not **Q/2**, confirmed to 4 decimal places. Decision 2's `bpGain = 2*q1`
(intended to normalize `Q/2` to `1.0`) therefore actually normalized `Q` to `2.0`
— by coincidence still Q-independent (the wrong `/2` and the real factor of `Q`
happened to cancel), just at exactly twice the peak the design doc described.

**Fix:** `kResonanceGain` is now set to `5.6` — the *actual* old peak
(`2 × 2.8`), not the value the old formula's own comment claimed. The new
biquad's peak is a true `1.0×` by construction rather than an accidental
cancellation, so `kResonanceGain` alone is now the whole gain story. Verified via
`scripts/analyze_effects.py` field-by-field against the pre-fix build: at
default settings, output RMS and fundamental gain match to within measurement
noise (e.g. `fundamental_gain` 0.198954 → 0.198664); the ~400 fields that do
differ are all in off-resonance/sweep operating points where the filter's
frequency-response *shape* legitimately differs between the two topologies, not
in default loudness.

### Verification

- `tests/dsp/biquad_test.cpp` — the shared primitive's own accuracy/stability
  coverage.
- `tests/wah_svf_accuracy_probe.cpp` — old-vs-new comparison specific to
  wah.cpp's reachable fc×Q space.
- A dedicated stability sweep (extreme Q/wahPos/mix corners, both modes, rapid
  knob jumps every 16 blocks, 20× input overload) stayed bounded and finite
  throughout — no instability at the corner Decision 1's stability note used to
  worry about.
- `scripts/analyze_effects.py` before/after: 0 differences in every other
  effect; no new or lost qualitative flags for `wah`; default-settings loudness
  matches to within noise.

### Growl/output saturator: measured, declined

Bug 2's discovery — an actual old peak of 5.6×, not the 2.8× the original
comment claimed — meant the growl and output-limiter `tanhf` stages were
already saturating harder and more often than the original design intended.
The obvious next move, following this session's corpus study (no third-party
patch links a newlib transcendental; `Malleus_Fuzz` uses a cascaded
`x/(1+|x|)` rational saturator instead of `tanhf`), was to try the same swap
here.

**Measured, not assumed, and declined.** Substituting `x/(1+d|x|)` for both
`tanhf` call sites (same near-origin slope and asymptote, different
transition shape) and comparing via `scripts/analyze_effects.py` at identical
settings: `spectral.thd_percent` went from 0.044% to **1.65%** — a
reproducible 37x increase — with `residual_ratio` elevated by 37–109x across
the sweep. Both absolute figures are individually small, but the delta is
consistent and real, not noise. `effects/wah.cpp` keeps `tanhf`. Full writeup:
[`tests/wah_saturator_ab_probe.md`](../tests/wah_saturator_ab_probe.md).

---

## Related Files

- `effects/wah.cpp` — the patch implementation
- `docs/circuit-to-patch-conversion.md` — SVF primitive documentation, general methodology
- `docs/endless-reference.md` — full SDK reference, including current expression pedal routing
- `internal/PatchCppWrapper.cpp` — expression pedal hardcoded to param 2
- `docs/templates/patch-build-walkthrough.md` — blank template for future patches
- `source/dsp/biquad.h` / `source/dsp/filter_coeff.h` — the RBJ bandpass biquad
  primitive this patch now uses, and the warning on the Chamberlin `svfF1` it replaced
- `tests/wah_svf_accuracy_probe.cpp` — the before/after accuracy measurement
- `docs/endl-corpus-study.md` / `docs/reverse-engineering/` — the methodology
  this fix came from, originally built for `funk_machine_envelope_filter.cpp`
- `docs/funk-machine-envelope-filter-build-walkthrough.md` /
  `docs/harmonica-build-walkthrough.md` — the two sibling effects that shared
  this same detuning bug and received the same fix
