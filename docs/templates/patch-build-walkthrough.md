# [Patch Name] — Build Walkthrough

<!--
TEMPLATE INSTRUCTIONS (delete this block before committing)
===========================================================
This template is for documenting the build of a new Polyend Endless effects patch.
Fill in each section as you design and implement the patch.
See docs/wah-build-walkthrough.md for a completed example.

BEFORE STARTING — checklist:
[ ] Identify the effect's circuit or sonic reference (hardware pedal, synthesis technique)
[ ] Map the circuit blocks to DSP primitives (see docs/circuit-to-patch-conversion.md)
[ ] Count required state variables — do you need the working buffer, or are scalars enough?
[ ] Assign parameters: remember this repo's current wrapper exposes expression on param 2 (Right knob)
[ ] Decide each parameter's taper: linear, log, power-law, or bounded/compensated
[ ] Choose defaults that land on a usable sound, not just 0.5 by habit
[ ] Choose LED color scheme — verify enum values in source/Patch.h before coding
[ ] Run bash tests/check_patches.sh after writing the patch
-->

**File:** `effects/[filename].cpp`  
**Date:** YYYY-MM-DD  
**Filter/algorithm type:** [e.g., Chamberlin SVF, 1-pole IIR, biquad, delay line, etc.]  
**Reference circuit / inspiration:** [e.g., Dunlop Crybaby GCB-95, Tube Screamer TS-808, etc.]  
**Template:** [`docs/templates/patch-build-walkthrough.md`](patch-build-walkthrough.md)

---

## Overview

<!-- One short paragraph: what does this effect do, what hardware does it approximate,
     what makes it interesting or useful on the Endless? -->

---

## Circuit Reference

<!-- Describe the analog circuit or sonic model this patch approximates.
     For hardware circuits: list key component values, measured frequency ranges,
     gain figures, and qualitative character.
     For non-circuit effects: describe the algorithm basis (e.g., "Schroeder reverb",
     "bucket-brigade chorus") and key parameters.
     If there's a good analysis source (e.g., ElectroSmash), link it. -->

---

## Pre-Implementation Checklist

| Item | Answer |
|---|---|
| DSP primitive needed | [SVF / 1-pole IIR / Biquad / Delay / Waveshaper / ...] |
| Circuit-modeling approach | [Hand-tuned filters + tanh / SPICE-derived parameters + shaped nonlinearity / WDF circuit port] |
| Fork cross-check | [Relevant primitive / patch / cautionary tale in `docs/fork-comparisons/sthompsonjr-wdf.md`, and why we are (or are not) using it. "None applicable" is a valid answer; "did not check" is not.] |
| Working buffer needed? | [Yes (delay lines) / No (scalars only)] |
| State variable count | [N floats — list them] |
| Knob 0 (Left) | [parameter name] |
| Knob 1 (Mid) | [parameter name] |
| Knob 2 (Right) / Exp pedal | [parameter name — note: exp pedal controls this] |
| Active LED color | [`Color::k___`] |
| Bypassed LED color | [`Color::k___`] |
| Working buffer size needed | [N floats — or N/A] |

---

## Decision 1 — [Filter Topology / Algorithm]

**Goal:** [What property or behavior are you trying to achieve?]

**Options considered:**

| Option | Pros | Cons |
|---|---|---|
| Option A | ... | ... |
| Option B | ... | ... |

**Chose: [Option].** [One or two sentences explaining why.]

**Rejected:** [Other option] — [brief reason].

---

## Decision 1.5 — Circuit Modeling Fidelity

<!-- Before coding, decide how literally the patch should model the source.
     Recommended framing:
     - Hand-tuned filters + tanh: fastest path, best when the goal is "pedal-like feel"
       rather than schematic fidelity
     - SPICE-derived parameters + shaped nonlinearity: useful when a circuit analysis
       gives you meaningful breakpoints/Q/gain targets but a full circuit solver would
       be overkill
     - WDF circuit port: use only when the circuit topology itself is the sound and you
       have a reason to spend more CPU on it
     See docs/fork-comparisons/sthompsonjr-wdf.md for tradeoffs and examples.
     Record expected CPU pressure here: what runs per sample, what runs per block, and
     what you expect the most expensive math to be. -->

---

## Decision 2 — [Gain Staging / Normalization]

<!-- How does signal level flow through the patch?
     Where does gain happen? Where could it blow up?
     How is the output normalized to stay in (-1, +1)?
     See docs/circuit-to-patch-conversion.md §4 for gain staging principles. -->

---

## Decision 3 — Control Layout

<!-- How did you assign knobs to parameters?
     Was there a conflict (e.g., expression pedal routing)?
     How was it resolved?
     Document the final mapping with a table: -->

| Knob | Param | Function | Range | Default |
|---|---|---|---|---|
| Left | 0 | [name] | [min]–[max] | [default] |
| Mid | 1 | [name] | [min]–[max] | [default] |
| Right / Exp | 2 | [name] | [min]–[max] | [default] |

---

## Decision 4 — [Parameter Taper / Mapping]

<!-- Are any parameters on a log taper (frequency, gain)?
     Any unusual ranges?
     For each parameter, note:
     - why the taper is linear/log/power-law/bounded
     - whether the control does useful work across most of the knob travel
     - whether the default is a musical setting rather than a numeric midpoint
     - whether param 2 still makes sense when used by the expression pedal
     Document the formula used. -->

---

## Decision 4.5 — Primitive Reuse

<!-- Before adding another local helper, check whether an existing primitive already
     fits the job or would be a better starting point.
     Questions to answer:
     - Is this really a new DSP block, or another copy of clamp/taper/one-pole/mix-law code?
     - Would a shared primitive make the patch easier to test or review?
     - If the patch uses a custom helper anyway, why is reuse not the right choice here?
     See docs/fork-comparisons/sthompsonjr-wdf.md for the current duplication audit.
     Once this repo grows a shared source/dsp/ layer, link the exact primitive(s) used
     from here. -->

---

## Decision 5 — Footswitch UX

<!-- How are the two footswitch events used?
     - kLeftFootSwitchPress (idx=0): ?
     - kLeftFootSwitchHold (idx=1): ?
     Note: second physical footswitch is not accessible from patch code in current SDK. -->

---

## Decision 6 — LED Design

<!-- Document all LED states the patch can be in.
     Verify enum values exist in source/Patch.h before writing code. -->

| State | Color | `Color` enum |
|---|---|---|
| [state 1] | [color] | `Color::k___` |
| [state 2] | [color] | `Color::k___` |

---

## Decision 7 — State Clearing Strategy

<!-- When should filter/delay state be cleared to prevent pops?
     When should it NOT be cleared (to preserve continuity)?
     Document each event: -->

| Event | Clear state? | Reason |
|---|---|---|
| Bypass → active | [Yes/No] | ... |
| Active → bypass | [Yes/No] | ... |
| [Other event] | [Yes/No] | ... |

---

## Implementation Notes

### Working buffer allocation
<!-- If you use the working buffer, show how it is partitioned.
     E.g.: delayL = buf.data(); delayR = buf.data() + kDelayLen; -->

### CPU budget estimate
<!-- How many multiply-adds per sample? Is there anything expensive (sinf, powf, tanhf)?
     Where is it called (per sample vs. per buffer)? -->

### [Any other notable implementation detail]

---

## Testing

```bash
# From the repo root:
bash tests/check_patches.sh
```

**Manual listening checklist:**
- [ ] Each knob does useful audible work across most of its travel
- [ ] [Describe what to listen for at parameter extreme A]
- [ ] [Describe what to listen for at parameter extreme B]
- [ ] Bypass engages and disengages cleanly (no pop)
- [ ] LED changes correctly with bypass and mode (if applicable)
- [ ] Expression pedal sweeps smoothly (if applicable)

---

## Future Improvements

<!-- What would you add or change if you had more time or SDK features?
     Be specific — this helps future LLM sessions pick up where you left off. -->

1. [Improvement 1 — describe what and why]
2. [Improvement 2]

---

## Related Files

- `effects/[filename].cpp` — the patch implementation
- `docs/circuit-to-patch-conversion.md` — DSP primitive reference
- `docs/endless-reference.md` — full SDK reference
- `internal/PatchCppWrapper.cpp` — current repo-local expression routing
<!-- Add any additional links specific to this patch -->
