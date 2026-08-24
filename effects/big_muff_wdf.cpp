// Big Muff Pi-inspired fuzz for Polyend Endless
//
// Primary voice:
//   Ram's Head-inspired sustain fuzz using a hybrid WDF-style core: two cascaded
//   wave-solved clipping stages feeding a classic Muff-like LP/HP tone-stack blend.
//
// Alternate voice:
//   Hold morphs into a Tone Bypass-inspired mids-lift voice with a flatter, louder
//   post-clip response while preserving the same Endless-friendly control story.
//
// Controls:
//   Left  knob  — Sustain
//   Mid   knob  — Tone
//   Right knob  — Blend (also expression pedal in this fork)
//   Footswitch  — Press: bypass, Hold: Ram's Head / Tone Bypass toggle
//   LED         — Red/DarkRed Ram's Head, Magenta/DimCyan Tone Bypass

#include "../source/Patch.h"
#include "../source/dsp/clamp.h"
#include "../source/dsp/crossfade.h"
#include "../source/dsp/filter_coeff.h"
#include "../source/dsp/one_pole_filter.h"
#include "../source/dsp/parameter_smoother.h"
#include "../source/dsp/soft_limit.h"

#include <cmath>

namespace {
constexpr float kTwoPi  = 6.283185307f;
constexpr float kHalfPi = 1.57079632679f;
constexpr float kFs     = static_cast<float>(Patch::kSampleRate);

using dsp::hpCoeff;
using dsp::lpCoeff;
using dsp::clamp01;
using dsp::clampSigned;
using dsp::clampUnit;
using SmoothedValue = dsp::ParamSmoother;

// Per-effect tuning of the safety soft-limit: (0.92, 0.24, 0.08) gives a
// slightly higher knee and tighter knee shape than the (0.90, 0.25, 0.10)
// default. Kept here so the tuning is visible at the call site instead of
// duplicated inside a near-identical helper.
inline float softLimit(float value)
{
    return dsp::softLimit(value, 0.92f, 0.24f, 0.08f);
}
using OnePoleLowpass  = dsp::OnePoleLowpass;
using OnePoleHighpass = dsp::OnePoleHighpass;

class WdfAntiparallelDiodePair
{
public:
    float process(float incident, float portResistance, float strength, float thermal)
    {
        const float rp      = portResistance < 0.04f ? 0.04f : portResistance;
        const float g       = strength < 0.001f ? 0.001f : strength;
        const float thermalClamped = thermal < 0.05f ? 0.05f : thermal;

        incident = clampSigned(incident, 2.8f);

        float voltage = clampSigned(lastVoltage_, 2.0f);
        for (int iter = 0; iter < 4; ++iter)
        {
            const float norm = clampSigned(voltage / thermalClamped, 11.0f);
            const float sh   = sinhf(norm);
            const float ch   = coshf(norm);
            const float f    = g * sh - (incident - voltage) / rp;
            const float df   = (g / thermalClamped) * ch + 1.0f / rp;
            voltage -= f / df;
            voltage = clampSigned(voltage, 2.0f);
        }

        lastVoltage_ = voltage;
        return voltage;
    }

    void reset()
    {
        lastVoltage_ = 0.0f;
    }

private:
    float lastVoltage_ = 0.0f;
};
} // namespace

class BigMuffWdfPatch final : public Patch
{
public:
    void init() override
    {
        sustain_.init(0.62f, 30.0f);
        tone_.init(0.48f, 24.0f);
        blend_.init(0.74f, 18.0f);
        toneBypassMorph_.init(0.0f, 34.0f);

        bypassed_   = false;
        toneBypass_ = false;

        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /* buffer */) override
    {
        // No external buffer required.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_) {
            return;
        }

        // lowToneAlpha/highToneLpAlpha take fixed literal cutoffs (245 Hz,
        // 1180 Hz) -- unlike bypassToneAlpha below, they don't depend on any
        // knob, so they only need to be computed once, not once per sample.
        const float lowToneAlpha    = lpCoeff(245.0f);
        const float highToneLpAlpha = lpCoeff(1180.0f);

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float sustainValue = clamp01(sustain_.process());
            const float toneValue    = clamp01(tone_.process());
            const float blendValue   = clamp01(blend_.process());
            const float bypassMorph  = clamp01(toneBypassMorph_.process());

            const float sustainCurve = 0.10f + 0.90f *
                                       (sustainValue * (1.05f - 0.05f * sustainValue));
            const float toneCurve    = toneValue * toneValue * (0.30f + 0.70f * toneValue);

            const float inputHpAlpha  = hpCoeff(18.0f + 34.0f * sustainCurve);
            const float stageHpAlpha  = hpCoeff(48.0f + 64.0f * sustainCurve);
            const float stage1LpAlpha = lpCoeff(1760.0f - 220.0f * sustainCurve);
            const float stage2LpAlpha = lpCoeff(1420.0f - 180.0f * sustainCurve);

            const float stage1Gain       = 3.10f + 5.50f * sustainCurve;
            const float stage1PortR      = 0.33f - 0.08f * sustainCurve;
            const float stage1Strength   = 0.08f + 0.05f * sustainCurve;
            const float stage1Thermal    = 0.17f - 0.02f * sustainCurve;
            const float stage2Gain       = 4.90f + 9.20f * sustainCurve;
            const float stage2PortR      = 0.26f - 0.05f * sustainCurve;
            const float stage2Strength   = 0.11f + 0.08f * sustainCurve;
            const float stage2Thermal    = 0.16f - 0.02f * sustainCurve;
            const float inputBoost       = 1.28f + 3.65f * sustainCurve;
            const float bypassToneAlpha  = lpCoeff(1220.0f + 3600.0f * toneCurve);
            const float toneLowWeight    = cosf(toneValue * kHalfPi);
            const float toneHighWeight   = sinf(toneValue * kHalfPi);
            const auto  blendGains       = dsp::equalPower(blendValue);
            const float dryGain          = blendGains.dry;
            const float wetGain          = blendGains.wet;
            const float wetMakeup        = 0.98f + 0.08f * blendValue;

            left[i]  = processSample(0, left[i], inputHpAlpha, stageHpAlpha, stage1LpAlpha,
                                     stage2LpAlpha, stage1Gain, stage1PortR,
                                     stage1Strength, stage1Thermal, stage2Gain,
                                     stage2PortR, stage2Strength, stage2Thermal,
                                     inputBoost, lowToneAlpha, highToneLpAlpha,
                                     bypassToneAlpha, toneCurve, toneLowWeight,
                                     toneHighWeight, dryGain, wetGain, wetMakeup,
                                     bypassMorph);
            right[i] = processSample(1, right[i], inputHpAlpha, stageHpAlpha, stage1LpAlpha,
                                     stage2LpAlpha, stage1Gain, stage1PortR,
                                     stage1Strength, stage1Thermal, stage2Gain,
                                     stage2PortR, stage2Strength, stage2Thermal,
                                     inputBoost, lowToneAlpha, highToneLpAlpha,
                                     bypassToneAlpha, toneCurve, toneLowWeight,
                                     toneHighWeight, dryGain, wetGain, wetMakeup,
                                     bypassMorph);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.62f}; // Sustain
            case 1: return {0.0f, 1.0f, 0.48f}; // Tone
            case 2: return {0.0f, 1.0f, 0.74f}; // Blend / expression
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    const char* getParameterName(int idx) override
    {
        switch (idx) {
            case 0: return "Sustain";
            case 1: return "Tone";
            case 2: return "Blend";
            default: return nullptr;
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: sustain_.setTarget(value); break;
            case 1: tone_.setTarget(value); break;
            case 2: blend_.setTarget(value); break;
            default: break;
        }
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress))
        {
            bypassed_ = !bypassed_;
            if (!bypassed_) {
                clearState();
            }
        }
        else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold))
        {
            toneBypass_ = !toneBypass_;
            toneBypassMorph_.setTarget(toneBypass_ ? 1.0f : 0.0f);
        }
    }

    Color getStateLedColor() override
    {
        if (toneBypass_) {
            return bypassed_ ? Color::kDimCyan : Color::kMagenta;
        }
        return bypassed_ ? Color::kDarkRed : Color::kRed;
    }

private:
    float processSample(int channel,
                        float input,
                        float inputHpAlpha,
                        float stageHpAlpha,
                        float stage1LpAlpha,
                        float stage2LpAlpha,
                        float stage1Gain,
                        float stage1PortR,
                        float stage1Strength,
                        float stage1Thermal,
                        float stage2Gain,
                        float stage2PortR,
                        float stage2Strength,
                        float stage2Thermal,
                        float inputBoost,
                        float lowToneAlpha,
                        float highToneLpAlpha,
                        float bypassToneAlpha,
                        float toneCurve,
                        float toneLowWeight,
                        float toneHighWeight,
                        float dryGain,
                        float wetGain,
                        float wetMakeup,
                        float bypassMorph)
    {
        const float dry = input;

        const float conditioned = inputHp_[channel].process(input, inputHpAlpha) * inputBoost;
        const float stage1Band  = stage1Lp_[channel].process(conditioned, stage1LpAlpha);
        const float stage1Clip =
          stage1Clipper_[channel].process(stage1Band * stage1Gain + conditioned * 0.22f,
                                          stage1PortR, stage1Strength, stage1Thermal);
        const float stage1Out = tanhf((stage1Clip + stage1Band * 0.18f) * 1.20f);

        const float interstage = stageHp_[channel].process(stage1Out, stageHpAlpha);
        const float stage2Band = stage2Lp_[channel].process(interstage, stage2LpAlpha);
        const float stage2Clip =
          stage2Clipper_[channel].process(stage2Band * stage2Gain + interstage * 0.30f,
                                          stage2PortR, stage2Strength, stage2Thermal);
        const float stage2Out = tanhf((stage2Clip + stage2Band * 0.16f) * 1.28f);

        const float fuzzCore = clampUnit(stage1Out * 0.25f + stage2Out * 0.95f);

        const float lowBranch = toneLowLp_[channel].process(fuzzCore, lowToneAlpha);
        const float highBase  = toneHighLp_[channel].process(fuzzCore, highToneLpAlpha);
        const float highBranch = fuzzCore - highBase;
        const float normalVoiced = softLimit(
          (lowBranch * (0.95f * toneLowWeight) +
           highBranch * (1.08f * toneHighWeight)) * 1.02f);

        const float bypassBase = bypassToneLp_[channel].process(fuzzCore, bypassToneAlpha);
        const float bypassEdge = fuzzCore - bypassBase;
        const float bypassVoiced = softLimit(
          (fuzzCore * 0.74f +
           bypassBase * 0.56f +
           bypassEdge * (0.22f + 0.42f * toneCurve)) * 1.08f);

        const float wet =
          normalVoiced * (1.0f - bypassMorph) + bypassVoiced * bypassMorph;
        const float mixed = dry * dryGain + wet * wetGain * wetMakeup;
        return softLimit(mixed);
    }

    void clearState()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHp_[ch].reset();
            stageHp_[ch].reset();
            stage1Lp_[ch].reset();
            stage2Lp_[ch].reset();
            toneLowLp_[ch].reset();
            toneHighLp_[ch].reset();
            bypassToneLp_[ch].reset();
            stage1Clipper_[ch].reset();
            stage2Clipper_[ch].reset();
        }
    }

    SmoothedValue sustain_;
    SmoothedValue tone_;
    SmoothedValue blend_;
    SmoothedValue toneBypassMorph_;

    bool bypassed_   = false;
    bool toneBypass_ = false;

    OnePoleHighpass inputHp_[2];
    OnePoleHighpass stageHp_[2];
    OnePoleLowpass  stage1Lp_[2];
    OnePoleLowpass  stage2Lp_[2];
    OnePoleLowpass  toneLowLp_[2];
    OnePoleLowpass  toneHighLp_[2];
    OnePoleLowpass  bypassToneLp_[2];
    WdfAntiparallelDiodePair stage1Clipper_[2];
    WdfAntiparallelDiodePair stage2Clipper_[2];
};

Patch* Patch::getInstance()
{
    static BigMuffWdfPatch instance;
    return &instance;
}
