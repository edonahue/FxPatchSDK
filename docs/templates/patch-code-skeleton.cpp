// Patch code skeleton for Polyend Endless
//
// Copy this file into effects/ and rename it for the patch you are building.
//
// TODO: replace the placeholder patch name, voice description, control mapping, and LED
// scheme with the patch-specific design. Pair this with
// docs/templates/patch-build-walkthrough.md while you implement the effect.
//
// Primary voice:
//   TODO: describe the default sound and the core circuit/DSP idea.
//
// Alternate voice:
//   TODO: describe the long-press mode, if the patch has one.
//
// Controls:
//   Left  knob  — TODO
//   Mid   knob  — TODO
//   Right knob  — TODO (also expression pedal in this repo's current wrapper)
//   Footswitch  — Press: bypass, Hold: alternate voice toggle
//   LED         — TODO: document active/bypassed colors for each voice

#include "../source/Patch.h"

#include <cmath>

namespace {
// These helpers mirror the style already used in effects/*.cpp. If this repo grows a
// shared source/dsp/ layer, move these into that shared code instead of redefining them.
[[maybe_unused]] constexpr float kFs     = static_cast<float>(Patch::kSampleRate);
[[maybe_unused]] constexpr float kTwoPi  = 6.283185307f;
[[maybe_unused]] constexpr float kHalfPi = 1.57079632679f;

[[maybe_unused]] float clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

[[maybe_unused]] float clampUnit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

[[maybe_unused]] float hpCoeff(float fc)
{
    return 1.0f / (1.0f + kTwoPi * fc / kFs);
}

[[maybe_unused]] float lpCoeff(float fc)
{
    const float omega = kTwoPi * fc / kFs;
    return omega / (1.0f + omega);
}

[[maybe_unused]] float equalPowerDry(float mix)
{
    return cosf(clamp01(mix) * kHalfPi);
}

[[maybe_unused]] float equalPowerWet(float mix)
{
    return sinf(clamp01(mix) * kHalfPi);
}
} // namespace

class PatchImpl final : public Patch
{
  public:
    void init() override
    {
        left_  = 0.5f;
        mid_   = 0.5f;
        right_ = 0.5f;

        bypassed_       = false;
        alternateVoice_ = false;

        // TODO: reset filter state, delay write pointers, envelopes, LFO phase, and any
        // other per-voice state here.
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        // TODO: partition the external buffer only if the patch needs long delay lines,
        // tables, grains, or other bulk storage.
        //
        // Example:
        // delayL_ = buffer.data();
        // delayR_ = buffer.data() + kDelayLen;
        (void)buffer;
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        // Current SDK buffers are in-place. Leaving samples unchanged is a passthrough.
        if (bypassed_) {
            return;
        }

        const float leftParam  = clamp01(left_);
        const float midParam   = clamp01(mid_);
        const float rightParam = clamp01(right_);
        (void)leftParam;
        (void)midParam;
        (void)rightParam;

        for (size_t i = 0; i < left.size(); ++i) {
            const float inL = left[i];
            const float inR = right[i];

            float outL = inL;
            float outR = inR;

            // TODO: DSP here.
            // Example structure:
            // 1. map params with musical tapers
            // 2. update coefficients outside the inner loop when possible
            // 3. process core nonlinear/filter/delay path
            // 4. clamp or soft-limit only as needed

            left[i]  = clampUnit(outL);
            right[i] = clampUnit(outR);
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (paramIdx) {
        case 0:
            return {0.0f, 1.0f, 0.5f};
        case 1:
            return {0.0f, 1.0f, 0.5f};
        case 2:
            return {0.0f, 1.0f, 0.5f};
        default:
            return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (paramIdx) {
        case 0:
            left_ = value;
            break;
        case 1:
            mid_ = value;
            break;
        case 2:
            right_ = value;
            break;
        default:
            break;
        }
    }

    void handleAction(int actionIdx) override
    {
        switch (actionIdx) {
        case static_cast<int>(endless::ActionId::kLeftFootSwitchPress):
            bypassed_ = !bypassed_;
            break;
        case static_cast<int>(endless::ActionId::kLeftFootSwitchHold):
            alternateVoice_ = !alternateVoice_;
            // TODO: decide whether changing voices should clear or preserve state.
            break;
        default:
            break;
        }
    }

    Color getStateLedColor() override
    {
        if (alternateVoice_) {
            return bypassed_ ? Color::kDimWhite : Color::kBeige;
        }
        return bypassed_ ? Color::kDimBlue : Color::kLightBlueColor;
    }

  private:
    // No heap allocations: keep state in members and use the working buffer explicitly.
    float left_  = 0.5f;
    float mid_   = 0.5f;
    float right_ = 0.5f;

    bool bypassed_       = false;
    bool alternateVoice_ = false;

    // TODO: add patch state here.
    // float z1_ = 0.0f;
    // float z2_ = 0.0f;
    // float* delayL_ = nullptr;
    // float* delayR_ = nullptr;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}
