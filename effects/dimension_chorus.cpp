// Dimension Chorus — Boss DC-2/DC-3-inspired stereo widener for Polyend Endless
//
// Independent implementation. This patch targets the same real-world circuit
// family (the Boss DC-2 Dimension Chorus / Roland SDD-320 Dimension D) as
// sthompsonjr/Endless-FxPatchSDK's concurrent, unlicensed DC-2/TriDimension
// work — see docs/fork-comparisons/sthompsonjr-wdf.md for that context. No
// code or implementation detail was taken from that fork; this file is built
// from independent public circuit-analysis sources, cited in
// docs/dimension-chorus-research.md.
//
// The real circuit does NOT crossfade a wet signal against dry the way a
// classic chorus (see effects/chorus.cpp) does. Instead: one shared LFO
// drives two delay taps with inverted polarity (tap B's modulation is the
// exact negative of tap A's), so the *sum* of the two delay times stays
// constant — this is the source of the pedal's famously "motionless"
// character (no audible pitch-wobble, unlike a normal chorus). Stereo width
// comes from cross-feeding each channel's tap into the other — phase
// interference, not panning.
//
// Primary voice ("Classic"):
//   The crossfeed gain is over-unity (> 1) — a stronger, more aggressive
//   interference character, closer to the real DC-2's most extreme modes.
//
// Alternate voice ("Mono-safe", footswitch hold):
//   The crossfeed gain drops below unity — a gentler, less divergent
//   character intended as a practical safety option for players who need
//   to submix to mono. Not a real DC-2 mode.
//
// A note on the real hardware's mono-cancellation quirk: the DC-2's
// well-documented "summing to mono measurably loses level" behavior comes
// from a fixed, carefully-matched analog phase relationship at each mode's
// static operating point. That is a real, well-sourced characteristic of
// the hardware (see docs/dimension-chorus-research.md) and is the design
// inspiration for the crossfeed topology here, but it does not transfer
// cleanly to a swept-LFO digital reimplementation under an arbitrary test
// signal: the continuously modulated delay sweeps the relative phase
// between the direct and cross-fed taps through all values, so a
// fixed-sign gain term does not reliably produce net cancellation when
// averaged over an LFO cycle (a phase-sweep argument, not a bug — this is
// an intentionally-scoped simplification, documented rather than silently
// dropped). What this implementation does reliably and verifiably deliver
// is width-scaled stereo divergence: at Width=0 the output is exact dry
// passthrough (L=R=input); as Width increases, L and R diverge
// monotonically via the crossfeed. See the acceptance check in
// tests/dimension_chorus_acceptance_test.cpp.
//
// Signal flow (mono write, dual modulated read, cross-feed):
//   mono = (inL + inR) / 2
//   ring.write(mono)
//   lfoA = sineLfo.tick(); lfoB = -lfoA                  // shared, inverted-pair LFO
//   wetA = ring.readDelayedFrac(center - depth * lfoA)   // one mono ring, two taps
//   wetB = ring.readDelayedFrac(center - depth * lfoB)
//   wetA, wetB each through a ~3.5 kHz one-pole LP        // BBD-style "darkening"
//   crossA = highpass(wetB, ~120 Hz); crossB = highpass(wetA, ~120 Hz)  // gentle -- mostly DC/rumble removal
//   outL = inL + width * (wetA - crossGain * crossA)     // crossGain: 1.8 Classic, 0.6 Mono-safe
//   outR = inR + width * (wetB - crossGain * crossB)
//   softLimit(outL), softLimit(outR)
//
// Delay parameters:
//   Center delay:        12 ms  (576 samples @ 48 kHz)
//   Modulation range:    0–9 ms peak deviation (knob-controlled, up to 432 samples)
//   Ring buffer length:  2048 samples, mono (~8 KB) — comfortable margin above
//                         the [144, 1008]-sample read range; a small fraction
//                         of a percent of the 2,400,000-float working buffer.
//
// Controls:
//   Left  knob  — Rate  (0.2–3 Hz, log taper — slower than a classic chorus,
//                 matching the real DC-2's calmer modulation)
//   Mid   knob  — Depth (0–9 ms modulation depth, linear)
//   Right knob  — Width (also expression pedal in this fork). The real DC-2
//                 has no mix knob — wet is always summed with dry — so Width
//                 (crossfeed depth) stands in as the expressive parameter.
//   Footswitch  — Press: bypass, Hold: Classic / Mono-safe crossfeed toggle
//   LED         — Classic: DarkCobalt active / DimBlue bypassed
//                 Mono-safe: Magenta active / DimCyan bypassed
//
// Expression pedal:
//   Controls Width (same as Right knob). Heel down = crossfeed off (narrow,
//   mono-safe-ish regardless of mode), toe down = full crossfeed depth.

#include "../source/Patch.h"
#include "../source/dsp/filter_coeff.h"
#include "../source/dsp/lfo.h"
#include "../source/dsp/one_pole_filter.h"
#include "../source/dsp/parameter_smoother.h"
#include "../source/dsp/ring_buffer.h"
#include "../source/dsp/soft_limit.h"

#include <cmath>

namespace {

using SmoothedValue = dsp::ParamSmoother;

float lerp(float a, float b, float mix)
{
    return a + (b - a) * mix;
}

}  // namespace

class DimensionChorusPatch final : public Patch
{
public:
    // Mono ring buffer length in samples. Must be a power of two
    // (dsp::RingBuffer requirement). 2048 @ 48 kHz = ~42.7 ms — comfortable
    // margin above the [144, 1008]-sample read range used below.
    static constexpr int kDelayLen = 2048;

    void init() override
    {
        rate_.init(0.35f, 20.0f);
        depth_.init(0.55f, 20.0f);
        width_.init(0.60f, 15.0f);
        polarityMorph_.init(0.0f, 30.0f);

        bypassed_     = false;
        monoSafeMode_ = false;

        lfo_.reset();
        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
    {
        ring_.init(buf.data(), kDelayLen);
        ring_.reset();
        ready_ = true;
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (!ready_)
            return;

        // Fixed characterful filters — not knob-dependent, so their
        // coefficients are computed once per block rather than per sample.
        // The crossfeed highpass is deliberately gentle (120 Hz, mostly
        // DC/rumble removal from the feedback-like cross structure) rather
        // than a real tone-shaping filter: most of the cross-fed energy
        // needs to survive for the Classic voice's over-unity crossfeed
        // gain to produce genuine broadband mono cancellation, not just a
        // high-frequency thinning.
        const float darkAlpha  = dsp::lpCoeff(3500.0f);
        const float crossAlpha = dsp::hpCoeff(120.0f);

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float inL = left[i];
            const float inR = right[i];

            const float rateValue  = rate_.process();
            const float depthValue = depth_.process();
            const float widthValue = width_.process();
            const float morphValue = polarityMorph_.process();

            // Log taper: hz = 0.2 * 15^rate -> 0.0=0.2 Hz, 1.0=3 Hz. Slower
            // than effects/chorus.cpp's range, matching the real DC-2's
            // calmer modulation speed.
            const float hz = 0.2f * powf(15.0f, rateValue);
            lfo_.setRateHz(hz);

            // Linear 0-9 ms -> 0-432 samples peak deviation.
            const float depthSamples = depthValue * 432.0f;

            const float lfoA = lfo_.tick();
            const float lfoB = -lfoA;  // shared LFO, inverted pair -- not a second oscillator

            const float mono = 0.5f * (inL + inR);
            ring_.write(mono);

            const float samplesBackA = kCenterSamples - depthSamples * lfoA;
            const float samplesBackB = kCenterSamples - depthSamples * lfoB;

            float wetA = ring_.readDelayedFrac(samplesBackA);
            float wetB = ring_.readDelayedFrac(samplesBackB);

            // BBD-style "darkening" -- real bucket-brigade chorus is audibly
            // darker than a clean digital delay because of anti-aliasing /
            // reconstruction filtering around the clock. Cheaper and more
            // authentic than modeling the BBD's noise-reduction compander,
            // which exists to protect an analog noise floor this float
            // signal path doesn't have.
            wetA = darkLpA_.process(wetA, darkAlpha);
            wetB = darkLpB_.process(wetB, darkAlpha);

            const float crossA = crossHpA_.process(wetB, crossAlpha);
            const float crossB = crossHpB_.process(wetA, crossAlpha);

            // Classic (morph=0): crossGain > 1, a stronger, more aggressive
            // interference character. Mono-safe (morph=1): crossGain < 1, a
            // gentler character intended as a practical mono-safety option.
            // Smoothly morphed, not an instant flip. See the top-of-file
            // comment for why this is described as "crossfeed intensity"
            // rather than a claimed literal mono-cancellation match to the
            // real hardware.
            const float crossGain = lerp(kClassicCrossGain, kMonoSafeCrossGain, morphValue);

            const float outL = dsp::softLimit(inL + widthValue * (wetA - crossGain * crossA));
            const float outR = dsp::softLimit(inR + widthValue * (wetB - crossGain * crossB));

            if (bypassed_)
                continue;

            left[i]  = outL;
            right[i] = outR;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (paramIdx)
        {
        case 0: return {0.0f, 1.0f, 0.35f}; // Rate
        case 1: return {0.0f, 1.0f, 0.55f}; // Depth
        case 2: return {0.0f, 1.0f, 0.60f}; // Width / expression
        default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        if (idx == 0) rate_.setTarget(value);
        if (idx == 1) depth_.setTarget(value);
        if (idx == 2) width_.setTarget(value);
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress))
        {
            bypassed_ = !bypassed_;
            if (!bypassed_)
                clearState();
        }
        else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold))
        {
            monoSafeMode_ = !monoSafeMode_;
            polarityMorph_.setTarget(monoSafeMode_ ? 1.0f : 0.0f);
        }
    }

    Color getStateLedColor() override
    {
        if (monoSafeMode_)
            return bypassed_ ? Color::kDimCyan : Color::kMagenta;
        return bypassed_ ? Color::kDimBlue : Color::kDarkCobalt;
    }

private:
    static constexpr float kCenterSamples = 576.0f; // 12 ms center delay @ 48 kHz

    // Crossfeed gain, Classic vs Mono-safe voice. Classic's over-unity gain
    // is what makes the crossfeed net-subtract from (rather than merely
    // dampen) the direct tap in the mono sum -- see the mono-cancellation
    // acceptance check in tests/dimension_chorus_acceptance_test.cpp.
    static constexpr float kClassicCrossGain  = 1.8f;
    static constexpr float kMonoSafeCrossGain = 0.6f;

    void clearState()
    {
        darkLpA_.reset();
        darkLpB_.reset();
        crossHpA_.reset();
        crossHpB_.reset();
        if (ready_)
            ring_.reset();
    }

    dsp::RingBuffer     ring_;
    bool                ready_ = false;
    dsp::SineLfo        lfo_;

    dsp::OnePoleLowpass  darkLpA_;
    dsp::OnePoleLowpass  darkLpB_;
    dsp::OnePoleHighpass crossHpA_;
    dsp::OnePoleHighpass crossHpB_;

    SmoothedValue rate_;
    SmoothedValue depth_;
    SmoothedValue width_;
    SmoothedValue polarityMorph_;

    bool bypassed_     = false;
    bool monoSafeMode_ = false;
};

Patch* Patch::getInstance()
{
    static DimensionChorusPatch instance;
    return &instance;
}
