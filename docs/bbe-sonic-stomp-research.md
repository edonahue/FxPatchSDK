# BBE Sonic Stomp / Lumin Research Notes

This document summarizes the public research used to design a new Endless patch inspired
by the BBE Sonic Stomp / Sonic Maximizer family. The goal is not to reproduce the
proprietary analog chip literally, but to capture the public, behavior-level consensus
well enough to make a strong guitar-oriented Endless patch.

---

## High-Level Consensus

Across the official BBE materials, the Aion `Lumin` clone documentation, and the public
patent-level descriptions, the same broad picture appears:

- the effect is not a conventional EQ pedal
- the signal is divided into broad frequency regions
- those regions are treated differently in phase/time terms
- the treated regions are mixed back with the original program path
- the stock pedal exposes only `Contour` and `Process`
- popular clone interpretations add a third `Midrange` control by surfacing the bandpass path

The most practically useful public synthesis for pedal builders is Aion's `Lumin`
documentation, which frames the process as a three-part state-variable split and recombine
design rather than a magical black box.

## Public Sources Reviewed

- BBE Sonic Stomp user manual:
  <https://www.manualslib.com/manual/408931/Bbe-Sonic-Stomp.html>
- Aion `Lumin` legacy documentation PDF:
  <https://aionfx.com/app/files/docs/lumin_legacy_documentation.pdf>
- Aion project page:
  <https://aionfx.com/project/lumin-legacy/>
- Updated Aion project page:
  <https://aionfx.com/project/lumin-125b-sonic-maximizer/>
- BBE reference patent:
  <https://patents.google.com/patent/US4482866A/en>
- TC Electronic Mimiq Doubler user/manual references for the stretch-goal doubler concept:
  <https://www.manualslib.com/manual/1176920/Tc-Electronic-Mimiq-Doubler.html>
  <https://device.report/manual/1196613>

---

## What the Official Sonic Stomp Materials Establish

The official user manual is useful for control intent and stock pedal anchors:

- `Contour` regulates the amount of phase-corrected bass frequencies
- `Process` regulates the amount of phase correction
- the published spec anchors the low contour around `50 Hz`
- the published spec anchors the process control around `10 kHz`

Those numbers are important because most clone and DIY discussion starts by deciding
whether to keep them or retune them for guitar.

## What the Aion Lumin Clone Adds

The Aion documentation is the clearest publicly accessible builder-oriented explanation of
the circuit family:

- it describes the process as essentially a **state-variable filter**
- the split is framed as:
  - lowpass: `50 Hz and below`
  - highpass: `10 kHz and above`
  - bandpass: everything between
- `Contour`, `Process`, and `Midrange` each affect one of those regions
- the stock BBE frequencies are explicitly criticized as not always ideal for guitar
- Aion recommends lower high-frequency targets and higher low-frequency targets for guitar

The most useful implementation implication is that a **3-knob, guitar-oriented interpretation**
is not arbitrary; it is a well-established public clone path.

The Lumin build notes also explain the midrange control clearly:

- the stock circuit has a fixed bandpass contribution
- the clone replaces that fixed amount with a potentiometer
- the center position corresponds to the stock setting

That is the strongest public basis for making `Midrange` a core control in the Endless patch.

## What the Patent Suggests

The patent is not the stompbox schematic, but it is still useful for the conceptual model.
The most relevant public ideas are:

- a straight-through program path remains present
- separate low and high correction channels are mixed back in
- low correction is described as **lagging** in phase
- high correction is described as **leading** in phase
- the correction channels are independently adjustable

For a digital adaptation, that suggests a dry path plus band-specific correction deltas is
more faithful to the public concept than simply running the whole signal through shelves or
static EQ boosts.

## Guitar-Oriented Design Implications

The public clone ecosystem is consistent on one point: the stock `50 Hz` and `10 kHz`
anchors are broad, and many guitar rigs benefit from retuning.

For an Endless patch primarily targeting electric guitar:

- the low crossover should move upward from stock so `Contour` interacts more with the
  useful guitar/bass body range and less with sub-bass
- the high crossover should move downward from stock so `Process` affects real pick
  articulation and upper harmonics, not only extreme air
- the mid band should remain available because guitar rigs are especially sensitive to
  midrange scoop or recovery

That is why the implementation plan favors a **guitar-tuned Lumin-style enhancer**, not a
literal stock Sonic Stomp recreation.

## Expression Pedal Implications in This Repo

The current SDK wrapper in this repository routes the expression pedal to `param 2`.
For a strict 3-knob layout, that creates an unavoidable conflict:

- if `param 2` is `Midrange`, the expression pedal will control `Midrange`
- if expression should control a separate overall intensity parameter, that requires more
  than the current stock fork API allows

For this patch family, the most sensible future expression target is still **overall
enhancer intensity / mix**, but that is a follow-up SDK capability problem, not a reason to
drop the 3-knob layout in v1.

## Mimiq-Style Doubler Stretch Goal

Public Mimiq materials describe the key behavioral ingredients as:

- one to three extra guitar layers
- variation in timing, pick attack, and pitch
- a `Tightness` control governing how similar or varied the overdubs feel

For Endless, a realistic full Mimiq recreation is outside the scope of the core enhancer.
But the public control philosophy is useful:

- a subtle post-enhancer doubler mode can add width and thickness
- it should be optional and off by default
- it should use decorrelated short delays and light modulation/randomization, not obvious echo

This makes a footswitch-hold doubler toggle a viable stretch goal that does not disturb the
core 3-knob enhancer design.

## Practical Conclusions for the Endless Patch

The strongest evidence-based design direction is:

- use a three-band split/recombine architecture
- preserve a dry/program path
- apply band-specific timing/phase-style correction rather than static EQ only
- tune the split points for electric guitar, not full-range mastering use
- expose `Contour`, `Process`, and `Midrange`
- treat expression as a future SDK enhancement problem
- keep any doubler mode optional, subtle, and post-enhancer
