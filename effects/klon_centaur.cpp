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
    // outputBaseTrim raised (0.86/0.82 → 1.00/0.96) so the Output knob can reach
    // the Centaur's reputed +20+ dB of clean boost at max. The drive-compensation
    // factor in processAudio (0.15·gainCurve) still keeps the loudness from running
    // away when Gain is cranked alongside Output.
    if (toneMod) {
        return {
            110.0f, // clipHpHz
            300.0f, // shelfHz, lower than stock for a fuller tone response
            2100.0f,// cleanLowpassHz
            2500.0f,// clippedLowpassHz
            0.94f,  // clippedBrightMix
            1.62f,  // shelfMaxBoost
            0.66f,  // shelfMinCut
            1.00f   // outputBaseTrim
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
        0.96f   // outputBaseTrim
    };
}
}

class KlonCentaurPatch final : public Patch
{
public:
    void init() override
    {
        gain_       = 0.36f;
        treble_     = 0.52f;
        output_     = 0.60f;
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

        const float gainCurve   = 0.04f + 0.96f * powf(gainClamped, 1.05f);
        const float outputCurve = outputClamped * (0.45f + 0.55f * outputClamped);

        const VoiceParams voice = getVoiceParams(toneMod_);

        const float clipHpAlpha     = hpCoeff(voice.clipHpHz + 55.0f * gainCurve);
        const float cleanLpAlpha    = lpCoeff(voice.cleanLowpassHz);
        const float clippedLpAlpha  = lpCoeff(voice.clippedLowpassHz);
        const float shelfLpAlpha    = lpCoeff(voice.shelfHz);

        // The dual-gang gain control is the main Klon behavior to preserve:
        // low settings keep a strong clean path, higher settings favor the clipped branch.
        const float cleanMix = 0.72f - 0.58f * gainCurve;
        const float dirtyMix = 0.36f + 1.48f * gainCurve;

        const float baseDrive = 1.25f + 3.10f * gainCurve;
        const float clipDrive = 1.55f + 6.85f * gainCurve;
        const float cleanBodyBlend = 0.06f + 0.10f * (1.0f - gainCurve);
        const float dirtyPostDrive = 1.00f + 1.05f * gainCurve;
        const float sumTrim = 0.84f - 0.08f * gainCurve;

        const float shelfGain =
          voice.shelfMinCut + (voice.shelfMaxBoost - voice.shelfMinCut) * trebleClamped;
        // Output law widened: (0.08 + 1.95·outputCurve) gives ≈28 dB of usable
        // knob range (was ≈18 dB), matching the real Centaur's famously wide
        // Output pot. Combined with the raised outputBaseTrim above, this lets
        // the pedal operate as a transparent clean boost at low Gain and reach
        // commercial-pedal loudness into the amp at max.
        const float outputGain =
          (0.08f + 1.95f * outputCurve) * (voice.outputBaseTrim - 0.10f * gainCurve);

        for (size_t i = 0; i < left.size(); ++i)
        {
            left[i]  = processSample(0, left[i], clipHpAlpha, cleanLpAlpha, clippedLpAlpha,
                                     shelfLpAlpha, cleanMix, dirtyMix, baseDrive, clipDrive,
                                     voice.clippedBrightMix, cleanBodyBlend, dirtyPostDrive,
                                     sumTrim, shelfGain, outputGain);
            right[i] = processSample(1, right[i], clipHpAlpha, cleanLpAlpha, clippedLpAlpha,
                                     shelfLpAlpha, cleanMix, dirtyMix, baseDrive, clipDrive,
                                     voice.clippedBrightMix, cleanBodyBlend, dirtyPostDrive,
                                     sumTrim, shelfGain, outputGain);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.36f}; // Gain
            case 1: return {0.0f, 1.0f, 0.52f}; // Treble
            case 2: return {0.0f, 1.0f, 0.60f}; // Output / expression
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
                        float cleanBodyBlend,
                        float dirtyPostDrive,
                        float sumTrim,
                        float shelfGain,
                        float outputGain)
    {
        const float buffered = input * baseDrive;

        cleanLp_[channel] += cleanLpAlpha * (buffered - cleanLp_[channel]);
        const float cleanPath = input * cleanMix + cleanLp_[channel] * cleanBodyBlend;

        float clippedPre =
          clipHpAlpha * (clipHpPrev_[channel] + buffered - clipInputPrev_[channel]);
        clipInputPrev_[channel] = buffered;
        clipHpPrev_[channel] = clippedPre;

        const float clipped = tanhf((clippedPre + buffered * 0.12f) * clipDrive);
        clippedLp_[channel] += clippedLpAlpha * (clipped - clippedLp_[channel]);
        const float dirtyCore =
          tanhf((clipped * dirtyPostDrive + clippedLp_[channel] * 0.38f) *
                (1.0f + 0.42f * dirtyMix));

        const float dirtyPath =
          dirtyCore * dirtyMix * clippedBrightMix + clippedLp_[channel] * dirtyMix * 0.12f;

        // Sum the cleaner and clipped branches before the active treble shelf.
        const float summed = clampUnit((cleanPath + dirtyPath) * sumTrim);

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
