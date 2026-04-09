// Klon Centaur-inspired transparent overdrive for Polyend Endless
//
// Primary voice:
//   Original Centaur-inspired low-gain overdrive with:
//   - buffered, clean-feeling front end
//   - gain-dependent clean/dirty rebalance
//   - germanium-style clipped branch summed back with a cleaner path
//   - active treble shelving and a strong output stage
//
// Alternate voice:
//   Hold toggles a Tone Mod-style variant inspired by the common C14 mod:
//   a fuller, less thin treble response with a slightly lower shelving corner.
//
// Controls:
//   Left  knob  — Gain
//   Mid   knob  — Treble
//   Right knob  — Output (also expression pedal in this fork)
//   Footswitch  — Press: bypass, Hold: stock / tone mod toggle
//   LED         — LightYellow/DimYellow stock, Beige/DimWhite tone mod

#include "../source/Patch.h"

#include <cmath>

namespace {
constexpr float kTwoPi = 6.283185307f;
constexpr float kFs    = static_cast<float>(Patch::kSampleRate);

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

struct VoiceParams
{
    float clipHpHz;
    float shelfHz;
    float cleanLowpassHz;
    float clippedLowpassHz;
    float clippedBrightMix;
    float shelfMaxBoost;
    float shelfMinCut;
    float outputBaseTrim;
};

VoiceParams getVoiceParams(bool toneMod)
{
    if (toneMod) {
        return {
            110.0f, // clipHpHz
            300.0f, // shelfHz, lower than stock for a fuller tone response
            2100.0f,// cleanLowpassHz
            2500.0f,// clippedLowpassHz
            0.94f,  // clippedBrightMix
            1.62f,  // shelfMaxBoost
            0.66f,  // shelfMinCut
            0.86f   // outputBaseTrim
        };
    }

    return {
        135.0f, // clipHpHz
        408.0f, // shelfHz
        1800.0f,// cleanLowpassHz
        2200.0f,// clippedLowpassHz
        0.88f,  // clippedBrightMix
        1.55f,  // shelfMaxBoost
        0.58f,  // shelfMinCut
        0.82f   // outputBaseTrim
    };
}
}

class KlonCentaurPatch final : public Patch
{
public:
    void init() override
    {
        gain_       = 0.28f;
        treble_     = 0.52f;
        output_     = 0.66f;
        bypassed_   = false;
        toneMod_    = false;

        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /* buf */) override
    {
        // No working buffer needed.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_) {
            return;
        }

        const float gainClamped   = clamp01(gain_);
        const float trebleClamped = clamp01(treble_);
        const float outputClamped = clamp01(output_);

        const float gainCurve   = powf(gainClamped, 0.82f);
        const float outputCurve = outputClamped * (0.45f + 0.55f * outputClamped);

        const VoiceParams voice = getVoiceParams(toneMod_);

        const float clipHpAlpha     = hpCoeff(voice.clipHpHz + 55.0f * gainCurve);
        const float cleanLpAlpha    = lpCoeff(voice.cleanLowpassHz);
        const float clippedLpAlpha  = lpCoeff(voice.clippedLowpassHz);
        const float shelfLpAlpha    = lpCoeff(voice.shelfHz);

        // The dual-gang gain control is the main Klon behavior to preserve:
        // low settings keep a strong clean path, higher settings favor the clipped branch.
        const float cleanMix = 0.88f - 0.68f * gainCurve;
        const float dirtyMix = 0.18f + 0.96f * gainCurve;

        const float baseDrive = 1.05f + 2.10f * gainCurve;
        const float clipDrive = 1.20f + 4.40f * gainCurve;

        const float shelfGain =
          voice.shelfMinCut + (voice.shelfMaxBoost - voice.shelfMinCut) * trebleClamped;
        const float outputGain =
          (0.22f + 1.95f * outputCurve) * (voice.outputBaseTrim - 0.08f * gainCurve);

        for (size_t i = 0; i < left.size(); ++i)
        {
            left[i]  = processSample(0, left[i], clipHpAlpha, cleanLpAlpha, clippedLpAlpha,
                                     shelfLpAlpha, cleanMix, dirtyMix, baseDrive, clipDrive,
                                     voice.clippedBrightMix, shelfGain, outputGain);
            right[i] = processSample(1, right[i], clipHpAlpha, cleanLpAlpha, clippedLpAlpha,
                                     shelfLpAlpha, cleanMix, dirtyMix, baseDrive, clipDrive,
                                     voice.clippedBrightMix, shelfGain, outputGain);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.28f}; // Gain
            case 1: return {0.0f, 1.0f, 0.52f}; // Treble
            case 2: return {0.0f, 1.0f, 0.66f}; // Output / expression
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: gain_   = value; break;
            case 1: treble_ = value; break;
            case 2: output_ = value; break;
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
            toneMod_ = !toneMod_;
            clearState();
        }
    }

    Color getStateLedColor() override
    {
        if (toneMod_) {
            return bypassed_ ? Color::kDimWhite : Color::kBeige;
        }
        return bypassed_ ? Color::kDimYellow : Color::kLightYellow;
    }

private:
    float processSample(int channel,
                        float input,
                        float clipHpAlpha,
                        float cleanLpAlpha,
                        float clippedLpAlpha,
                        float shelfLpAlpha,
                        float cleanMix,
                        float dirtyMix,
                        float baseDrive,
                        float clipDrive,
                        float clippedBrightMix,
                        float shelfGain,
                        float outputGain)
    {
        const float buffered = input * baseDrive;

        cleanLp_[channel] += cleanLpAlpha * (buffered - cleanLp_[channel]);
        const float cleanPath = input * cleanMix + cleanLp_[channel] * 0.12f;

        float clippedPre =
          clipHpAlpha * (clipHpPrev_[channel] + buffered - clipInputPrev_[channel]);
        clipInputPrev_[channel] = buffered;
        clipHpPrev_[channel] = clippedPre;

        const float clipped = tanhf(clippedPre * clipDrive);
        clippedLp_[channel] += clippedLpAlpha * (clipped - clippedLp_[channel]);

        const float dirtyPath =
          clipped * dirtyMix * clippedBrightMix + clippedLp_[channel] * dirtyMix * 0.24f;

        // Sum the cleaner and clipped branches before the active treble shelf.
        const float summed = clampUnit(cleanPath + dirtyPath);

        shelfLp_[channel] += shelfLpAlpha * (summed - shelfLp_[channel]);
        const float lowBranch  = shelfLp_[channel];
        const float highBranch = summed - lowBranch;

        const float voiced = lowBranch + highBranch * shelfGain;
        return clampUnit(voiced * outputGain);
    }

    void clearState()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            cleanLp_[ch]      = 0.0f;
            clipHpPrev_[ch]   = 0.0f;
            clipInputPrev_[ch]= 0.0f;
            clippedLp_[ch]    = 0.0f;
            shelfLp_[ch]      = 0.0f;
        }
    }

    float gain_   = 0.28f;
    float treble_ = 0.52f;
    float output_ = 0.66f;

    bool bypassed_ = false;
    bool toneMod_  = false;

    float cleanLp_[2]       = {0.0f, 0.0f};
    float clipHpPrev_[2]    = {0.0f, 0.0f};
    float clipInputPrev_[2] = {0.0f, 0.0f};
    float clippedLp_[2]     = {0.0f, 0.0f};
    float shelfLp_[2]       = {0.0f, 0.0f};
};

Patch* Patch::getInstance()
{
    static KlonCentaurPatch instance;
    return &instance;
}
