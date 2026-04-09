// Big Muff Pi-inspired fuzz for Polyend Endless
//
// Primary voice:
//   Ram's Head-inspired transistor Big Muff feel with:
//   - cascaded clipping stages
//   - the classic scooped LP/HP tone-stack blend
//   - strong sustain without relying on literal analog pot laws
//
// Alternate voice:
//   Hold toggles a Tone Bypass-inspired mids-lift mode that removes the classic
//   Muff scoop and replaces it with a flatter, louder post-clip voicing.
//
// Controls:
//   Left  knob  — Sustain
//   Mid   knob  — Tone
//   Right knob  — Blend (also expression pedal in this fork)
//   Footswitch  — Press: bypass, Hold: Ram's Head / Tone Bypass toggle
//   LED         — Red/DarkRed Ram's Head, Magenta/DimCyan Tone Bypass

#include "../source/Patch.h"

#include <cmath>

namespace {
constexpr float kTwoPi  = 6.283185307f;
constexpr float kHalfPi = 1.57079632679f;
constexpr float kFs     = static_cast<float>(Patch::kSampleRate);

float clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float clampUnit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float hpCoeff(float fc)
{
    return 1.0f / (1.0f + kTwoPi * fc / kFs);
}

float lpCoeff(float fc)
{
    const float omega = kTwoPi * fc / kFs;
    return omega / (1.0f + omega);
}

float equalPowerDry(float blend)
{
    return cosf(clamp01(blend) * kHalfPi);
}

float equalPowerWet(float blend)
{
    return 0.94f * sinf(clamp01(blend) * kHalfPi);
}
}

class BigMuffPatch final : public Patch
{
public:
    void init() override
    {
        sustain_ = 0.62f;
        tone_    = 0.48f;
        blend_   = 0.74f;

        bypassed_   = false;
        toneBypass_ = false;

        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /* buf */) override
    {
        // No external buffer required.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_) {
            return;
        }

        const float sustainClamped = clamp01(sustain_);
        const float toneClamped    = clamp01(tone_);
        const float blendClamped   = clamp01(blend_);

        const float sustainCurve = 0.12f + 0.88f * powf(sustainClamped, 1.15f);
        const float toneCurve    = powf(toneClamped, 1.10f);

        const float inputHpAlpha  = hpCoeff(18.0f + 38.0f * sustainCurve);
        const float stageHpAlpha  = hpCoeff(52.0f + 68.0f * sustainCurve);
        const float stage1LpAlpha = lpCoeff(1820.0f - 240.0f * sustainCurve);
        const float stage2LpAlpha = lpCoeff(1460.0f - 260.0f * sustainCurve);

        const float lowToneAlpha    = lpCoeff(260.0f);
        const float highToneLpAlpha = lpCoeff(1220.0f);
        const float bypassToneAlpha = lpCoeff(1300.0f + 3500.0f * toneCurve);

        const float inputBoost = 1.18f + 3.25f * sustainCurve;
        const float stage1Gain = 1.95f + 8.90f * sustainCurve;
        const float stage2Gain = 3.35f + 13.80f * powf(sustainCurve, 1.04f);

        const float toneLowWeight  = cosf(toneClamped * kHalfPi);
        const float toneHighWeight = sinf(toneClamped * kHalfPi);

        const float dryGain = equalPowerDry(blendClamped);
        const float wetGain = equalPowerWet(blendClamped);

        const float normalTrim = 0.84f - 0.04f * sustainCurve;
        const float bypassTrim = 0.98f - 0.05f * sustainCurve;

        for (size_t i = 0; i < left.size(); ++i)
        {
            left[i]  = processSample(0, left[i], inputHpAlpha, stageHpAlpha, stage1LpAlpha,
                                     stage2LpAlpha, lowToneAlpha, highToneLpAlpha, bypassToneAlpha,
                                     inputBoost, stage1Gain, stage2Gain, toneLowWeight, toneHighWeight,
                                     toneCurve, dryGain, wetGain, normalTrim, bypassTrim);
            right[i] = processSample(1, right[i], inputHpAlpha, stageHpAlpha, stage1LpAlpha,
                                     stage2LpAlpha, lowToneAlpha, highToneLpAlpha, bypassToneAlpha,
                                     inputBoost, stage1Gain, stage2Gain, toneLowWeight, toneHighWeight,
                                     toneCurve, dryGain, wetGain, normalTrim, bypassTrim);
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

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: sustain_ = value; break;
            case 1: tone_    = value; break;
            case 2: blend_   = value; break;
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
            clearState();
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
                        float lowToneAlpha,
                        float highToneLpAlpha,
                        float bypassToneAlpha,
                        float inputBoost,
                        float stage1Gain,
                        float stage2Gain,
                        float toneLowWeight,
                        float toneHighWeight,
                        float toneCurve,
                        float dryGain,
                        float wetGain,
                        float normalTrim,
                        float bypassTrim)
    {
        const float dry = input;

        float conditioned =
          inputHpAlpha * (inputHpPrev_[channel] + input - inputPrev_[channel]);
        inputPrev_[channel]   = input;
        inputHpPrev_[channel] = conditioned;
        conditioned *= inputBoost;

        stage1Lp_[channel] += stage1LpAlpha * (conditioned - stage1Lp_[channel]);
        const float clip1 = tanhf(stage1Lp_[channel] * stage1Gain);

        float interstage =
          stageHpAlpha * (stageHpPrev_[channel] + clip1 - stagePrev_[channel]);
        stagePrev_[channel]   = clip1;
        stageHpPrev_[channel] = interstage;

        stage2Lp_[channel] += stage2LpAlpha * (interstage - stage2Lp_[channel]);
        const float clip2 = tanhf(stage2Lp_[channel] * stage2Gain);

        const float fuzzCore = clampUnit(clip1 * 0.28f + clip2 * 0.92f);

        float voiced = 0.0f;
        if (!toneBypass_)
        {
            toneLowLp_[channel] += lowToneAlpha * (fuzzCore - toneLowLp_[channel]);
            toneHighLp_[channel] += highToneLpAlpha * (fuzzCore - toneHighLp_[channel]);

            const float lowBranch  = toneLowLp_[channel];
            const float highBranch = fuzzCore - toneHighLp_[channel];

            // Classic Muff-style passive tone-stack behavior: LP/HP blend with
            // the strongest scoop around the midpoint.
            voiced = lowBranch * (0.92f * toneLowWeight) +
                     highBranch * (1.08f * toneHighWeight);
            voiced = tanhf(voiced * 1.32f) * normalTrim;
        }
        else
        {
            // Tone-bypass-inspired mode: remove the classic scoop and replace it
            // with a flatter, louder path. Tone still acts as a gentle top-end control.
            bypassToneLp_[channel] += bypassToneAlpha * (fuzzCore - bypassToneLp_[channel]);
            const float edge = fuzzCore - bypassToneLp_[channel];

            voiced = fuzzCore * 0.72f +
                     bypassToneLp_[channel] * 0.58f +
                     edge * (0.16f + 0.44f * toneCurve);
            voiced = tanhf(voiced * 1.20f) * bypassTrim;
        }

        const float mixed = dry * dryGain + voiced * wetGain;
        return clampUnit(mixed);
    }

    void clearState()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            inputHpPrev_[ch]   = 0.0f;
            inputPrev_[ch]     = 0.0f;
            stageHpPrev_[ch]   = 0.0f;
            stagePrev_[ch]     = 0.0f;
            stage1Lp_[ch]      = 0.0f;
            stage2Lp_[ch]      = 0.0f;
            toneLowLp_[ch]     = 0.0f;
            toneHighLp_[ch]    = 0.0f;
            bypassToneLp_[ch]  = 0.0f;
        }
    }

    float sustain_ = 0.56f;
    float tone_    = 0.48f;
    float blend_   = 0.74f;

    bool bypassed_   = false;
    bool toneBypass_ = false;

    float inputHpPrev_[2]  = {0.0f, 0.0f};
    float inputPrev_[2]    = {0.0f, 0.0f};
    float stageHpPrev_[2]  = {0.0f, 0.0f};
    float stagePrev_[2]    = {0.0f, 0.0f};
    float stage1Lp_[2]     = {0.0f, 0.0f};
    float stage2Lp_[2]     = {0.0f, 0.0f};
    float toneLowLp_[2]    = {0.0f, 0.0f};
    float toneHighLp_[2]   = {0.0f, 0.0f};
    float bypassToneLp_[2] = {0.0f, 0.0f};
};

Patch* Patch::getInstance()
{
    static BigMuffPatch instance;
    return &instance;
}
