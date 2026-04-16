// Tube Screamer-inspired overdrive for Polyend Endless
//
// Primary voice:
//   TS808-inspired mid-hump overdrive with frequency-selective clipping,
//   controlled bass before clipping, and a musical post-clip tone sweep.
//
// Alternate voice:
//   Hold toggles a TS9-style variant with a slightly brighter, tighter, more
//   forward upper-mid response while preserving the same control layout.
//
// Controls:
//   Left  knob  — Drive
//   Mid   knob  — Level
//   Right knob  — Tone (also expression pedal in this fork)
//   Footswitch  — Press: bypass, Hold: TS808 / TS9 toggle
//   LED         — LightGreen/DimGreen TS808, PastelGreen/DarkLime TS9

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

float softLimit(float value)
{
    const float absValue = fabsf(value);
    if (absValue <= 0.90f) {
        return value;
    }

    const float sign = value < 0.0f ? -1.0f : 1.0f;
    const float over = (absValue - 0.90f) / 0.25f;
    return sign * (0.90f + 0.10f * tanhf(over));
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

struct VoiceParams
{
    float preHpHz;
    float splitHz;
    float lowBlend;
    float edgeDrive;
    float clipGain;
    float toneBaseHz;
    float toneRangeHz;
    float brightMix;
    float outputTrim;
};

VoiceParams getVoiceParams(bool ts9)
{
    // Third pass: pull the internal ceiling back down so Level can behave more
    // like a real pedal output control instead of immediately leaning on the DAC.
    if (ts9) {
        return {
            155.0f, // preHpHz
            760.0f, // splitHz
            0.78f,  // lowBlend
            2.85f,  // edgeDrive
            3.35f,  // clipGain
            1500.0f,// toneBaseHz
            6100.0f,// toneRangeHz
            1.18f,  // brightMix
            0.82f   // outputTrim
        };
    }

    return {
        135.0f, // preHpHz
        720.0f, // splitHz
        0.86f,  // lowBlend
        2.55f,  // edgeDrive
        3.00f,  // clipGain
        1250.0f,// toneBaseHz
        5200.0f,// toneRangeHz
        1.05f,  // brightMix
        0.78f   // outputTrim
    };
}
}

class TubeScreamerPatch final : public Patch
{
public:
    void init() override
    {
        drive_    = 0.46f;
        level_    = 0.62f;
        tone_     = 0.48f;
        bypassed_ = false;
        ts9Mode_  = false;

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

        const float driveClamped = clamp01(drive_);
        const float levelClamped = clamp01(level_);
        const float toneClamped  = clamp01(tone_);

        const float driveCurve = 0.08f + 0.92f * powf(driveClamped, 1.05f);
        const float toneCurve  = powf(toneClamped, 1.08f);
        const float levelCurve = levelClamped * (0.45f + 0.55f * levelClamped);

        const VoiceParams voice = getVoiceParams(ts9Mode_);

        const float preHpAlpha  = hpCoeff(voice.preHpHz + 40.0f * driveCurve);
        const float splitAlpha  = lpCoeff(voice.splitHz);
        const float toneAlpha   = lpCoeff(voice.toneBaseHz + voice.toneRangeHz * toneCurve);

        const float baseDrive = 1.55f + 3.15f * driveCurve;
        const float edgeDrive = 1.70f + voice.edgeDrive * (0.85f + 0.90f * driveCurve);
        const float clipGain  = 1.45f + voice.clipGain * (0.72f + 1.35f * driveCurve);
        const float postDrive = 1.05f + 1.30f * driveCurve;
        const float bodyKeep  = 0.06f + 0.10f * (1.0f - driveCurve);

        const float lowToneWeight  = cosf(toneClamped * kHalfPi);
        const float highToneWeight = sinf(toneClamped * kHalfPi);

        // Third pass: re-center the practical unity point near noon and leave
        // digital headroom for the Level control to act as an actual post-drive
        // output stage. The earlier hotter law reached unity too early and spent
        // the upper half of the knob shoving an already-dense signal into the
        // output clamp. This lower-span law keeps heel useful, noon near bypass
        // level, and toe available for boost.
        const float outputTrim =
          (0.08f + 0.88f * levelCurve) * (voice.outputTrim - 0.04f * driveCurve);

        for (size_t i = 0; i < left.size(); ++i)
        {
            left[i]  = processSample(0, left[i], preHpAlpha, splitAlpha, toneAlpha,
                                     baseDrive, edgeDrive, clipGain, lowToneWeight,
                                     highToneWeight, voice.lowBlend, voice.brightMix,
                                     postDrive, bodyKeep, outputTrim);
            right[i] = processSample(1, right[i], preHpAlpha, splitAlpha, toneAlpha,
                                     baseDrive, edgeDrive, clipGain, lowToneWeight,
                                     highToneWeight, voice.lowBlend, voice.brightMix,
                                     postDrive, bodyKeep, outputTrim);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.46f}; // Drive
            case 1: return {0.0f, 1.0f, 0.62f}; // Level
            case 2: return {0.0f, 1.0f, 0.48f}; // Tone / expression
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: drive_ = value; break;
            case 1: level_ = value; break;
            case 2: tone_  = value; break;
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
            ts9Mode_ = !ts9Mode_;
            clearState();
        }
    }

    Color getStateLedColor() override
    {
        if (ts9Mode_) {
            return bypassed_ ? Color::kDarkLime : Color::kPastelGreen;
        }
        return bypassed_ ? Color::kDimGreen : Color::kLightGreen;
    }

private:
    float processSample(int channel,
                        float input,
                        float preHpAlpha,
                        float splitAlpha,
                        float toneAlpha,
                        float baseDrive,
                        float edgeDrive,
                        float clipGain,
                        float lowToneWeight,
                        float highToneWeight,
                        float lowBlend,
                        float brightMix,
                        float postDrive,
                        float bodyKeep,
                        float outputTrim)
    {
        float conditioned =
          preHpAlpha * (hpPrev_[channel] + input - inputPrev_[channel]);
        inputPrev_[channel] = input;
        hpPrev_[channel] = conditioned;

        splitLp_[channel] += splitAlpha * (conditioned - splitLp_[channel]);
        const float body = splitLp_[channel];
        const float edge = conditioned - body;

        const float clipInput =
          body * (lowBlend * baseDrive) + edge * edgeDrive;
        const float clipped = tanhf(clipInput * clipGain);
        const float recovered =
          tanhf((clipped * postDrive + edge * 0.26f) * (1.0f + 0.55f * postDrive));

        // Keep the TS low-mid push, but let the clipped branch dominate much earlier
        // so lower-output guitars still read as overdrive instead of filtered clean boost.
        const float overdriveCore = clampUnit(recovered * 0.96f + body * bodyKeep);

        toneLp_[channel] += toneAlpha * (overdriveCore - toneLp_[channel]);
        const float lowBranch  = toneLp_[channel];
        const float highBranch = overdriveCore - toneLp_[channel];

        const float voiced =
          lowBranch * (0.94f * lowToneWeight) +
          highBranch * (brightMix * highToneWeight);

        return softLimit(voiced * outputTrim);
    }

    void clearState()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hpPrev_[ch]    = 0.0f;
            inputPrev_[ch] = 0.0f;
            splitLp_[ch]   = 0.0f;
            toneLp_[ch]    = 0.0f;
        }
    }

    float drive_ = 0.38f;
    float level_ = 0.62f;
    float tone_  = 0.48f;

    bool bypassed_ = false;
    bool ts9Mode_  = false;

    float hpPrev_[2]    = {0.0f, 0.0f};
    float inputPrev_[2] = {0.0f, 0.0f};
    float splitLp_[2]   = {0.0f, 0.0f};
    float toneLp_[2]    = {0.0f, 0.0f};
};

Patch* Patch::getInstance()
{
    static TubeScreamerPatch instance;
    return &instance;
}
