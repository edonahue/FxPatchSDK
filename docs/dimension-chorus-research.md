# Boss DC-2 Dimension Chorus Research Notes for Endless

Reviewed for this fork on 2026-06-14.

Primary sources:

- MusicRadar, "The FX files: BOSS DC-2 Dimension Chorus" — <https://www.musicradar.com/how-to/the-fx-files-boss-dc-2-dimension-chorus>
- Andertons Blog, "What is the Boss Dimension C Chorus and how does it work?" — <https://blog.andertons.co.uk/learn/what-is-the-boss-dimension-c-chorus-and-how-does-it-work>
- The Police Equipment Wiki, "Roland SDD-320 Dimension D" (the rack unit the
  DC-2 derives from) — <https://thepoliceequipmentwiki.miraheze.org/wiki/Roland_SDD-320_Dimension_D>
- Birth of a Synth, "Dim C/TZF Project" — <https://www.birthofasynth.com/Scott_Stites/Pages/dimc_main.html>

These are secondary sources, not primary schematics — several deep-dive
circuit-analysis pages (GGG, freestompboxes, AionFX, ElectroSmash) were
unreachable when this was researched. Treat the mechanism below as
well-corroborated (two-plus independent sources agree) rather than
schematic-verified.

A note on provenance: `sthompsonjr/Endless-FxPatchSDK` independently built a
DC-2/TriDimension circuit family around the same time as this research (see
[`docs/fork-comparisons/sthompsonjr-wdf.md`](fork-comparisons/sthompsonjr-wdf.md)).
That fork has no LICENSE file, so nothing here was taken from it — this
document and [`effects/dimension_chorus.cpp`](../effects/dimension_chorus.cpp)
are built entirely from the public sources listed above. That fork
independently arriving at the same circuit family is worth noting only as
corroboration this was a compelling target.

---

## What The Sources Establish Clearly

The DC-2 is architecturally different from a conventional chorus pedal, and
the sources agree on the shape even without primary schematics:

1. **Two BBD delay lines, one shared LFO, inverted clocking.** Both delay
   lines are always active together. One LFO drives both clocks, but line
   B's clock receives the *inverted* LFO signal relative to line A. Delay-A
   increases exactly as delay-B decreases and vice versa, so the **sum of
   the two delay times stays constant**. This is the source of the pedal's
   famous "motionless" character — no audible pitch-wobble, unlike a normal
   chorus where the average delay itself swims.
2. **Cross-feed with inversion and high-pass filtering.** Each output
   channel gets its own line's delayed signal directly, plus the *other*
   channel's delayed signal, inverted in polarity and passed through a
   high-pass filter. This cross-feed-with-inversion is the documented
   source of "width from phase interference" rather than a dry/wet pan.
3. **Modes vary intensity and speed, not topology.** The DC-2's four modes
   (1 subtlest → 4 most extreme) vary intensity and LFO speed; they do not
   switch between different delay lines or a different signal-flow shape.
4. **The mono-cancellation quirk.** Because the two wet paths are
   out-of-phase by design, higher-intensity modes suffer real, audible
   cancellation when summed to mono. This is a widely-repeated,
   well-corroborated fact about the DC-2, not folklore — and it is
   explicitly why Boss addressed mono-combine behavior in later designs
   using the same topology.
5. **BBD noise-reduction companding.** Standard analog BBD hygiene (an
   NE570-style compander pair) protects the bucket-brigade chip's limited
   dynamic range. This is not a creative element of the sound; it exists
   to solve an analog noise-floor problem this digital implementation
   doesn't share.

---

## What Matters for Endless

The real pedal has **no mix/blend knob** — wet is always summed with dry;
only mode (intensity) is selected. That immediately raises the control-law
question this repo cares about most: what goes on the expression-mapped
Right knob if there's no literal "Mix" to put there?

The answer used in [`effects/dimension_chorus.cpp`](../effects/dimension_chorus.cpp):
put **Width** (crossfeed intensity, which stands in for "mode 1 → mode 4")
on the Right knob. It's the single most expressive, performance-relevant
parameter, and sweeping it live with an expression pedal is a real gesture
a player would actually reach for — closer in spirit to the real pedal's
mode selector than a fabricated dry/wet blend would be.

The real BBD noise-reduction compander is explicitly **not** ported — see
the "Signal flow" comment block at the top of `dimension_chorus.cpp` for
why (it solves an analog noise-floor problem this float signal path
doesn't have, and would cost two extra transcendental calls per sample for
zero audible benefit here). A one-pole low-pass ("darkening") on each wet
tap is used instead — a cheaper, more directly-motivated stand-in for the
real audible effect of BBD anti-aliasing/reconstruction filtering.

---

## Main Design Risk — And Where This Implementation Draws The Line

The crossfeed-with-inversion mechanism is the real source of the pedal's
mono-cancellation character, but reproducing that character as a *provable,
testable property* under a digital, LFO-modulated reimplementation turned
out to be a genuine DSP-theoretic dead end during implementation: because
the delay is continuously swept, the relative phase between the direct and
cross-fed taps sweeps through all values over an LFO cycle, so a
fixed-sign crossfeed gain does not reliably net-cancel a broadband test
signal when averaged over that cycle (see the "note on the real hardware's
mono-cancellation quirk" in `effects/dimension_chorus.cpp`'s header comment
for the full derivation, and `tests/dimension_chorus_acceptance_test.cpp`
for the property that *is* reliably delivered and tested instead: width-
scaled stereo divergence, not literal mono-sum level loss).

This is documented honestly rather than silently smoothed over. The
crossfeed topology (over-unity gain in the "Classic" voice, under-unity in
"Mono-safe") is still directly inspired by the real mechanism and produces
a genuinely different, wider, more interference-driven character than
`effects/chorus.cpp`'s independent-delay-line approach — that architectural
difference is real and measured (see the build walkthrough for the actual
numbers). What isn't claimed is a literal, hardware-accurate reproduction
of the DC-2's specific mono-loss curve.
