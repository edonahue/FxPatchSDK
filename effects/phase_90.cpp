// Phase 90-inspired phaser for Polyend Endless
//
// Primary voice:
//   Block-logo Phase 90 style with a more pronounced, chewy sweep driven by
//   four all-pass stages plus the stronger feedback path.
//
// Alternate voice:
//   Hold toggles a script-mod / vintage voice with the feedback removed for a
//   smoother, softer sweep closer to the early script-logo units.
//
// Controls:
//   Mid knob    — Speed
//   Expression  — mirrors Speed in this fork because param 2 is hardwired
//   Footswitch  — Press: bypass, Hold: block / script toggle
//   LED         — Blue/DimBlue block, Beige/DimWhite script
//
// Notes:
//   - Left knob is intentionally unused
//   - Right knob is reserved for the expression lane; if turned without an
//     expression pedal attached it will mirror Speed rather than expose a
//     second public control

#include "../source/Patch.h"

#include <cmath>

namespace {
constexpr float kPi    = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kFs    = static_cast<float>(Patch::kSampleRate);

float clamp01(float x)
{
    if (x < 0.0f) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

float clampUnit(float x)
{
    if (x < -1.0f) {
        return -1.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

float triangleLfo(float phase)
{
    const float wrapped = phase - floorf(phase);
    return 1.0f - 4.0f * fabsf(wrapped - 0.5f);
}

float mapSpeedHz(float value)
{
    constexpr float kMinHz = 0.12f;
    constexpr float kMaxHz = 7.20f;
    const float ratio = kMaxHz / kMinHz;
    return kMinHz * powf(ratio, clamp01(value));
}

float mapSweepHz(float lfo, float minHz, float maxHz)
{
    const float norm = 0.5f * (lfo + 1.0f);
    const float ratio = maxHz / minHz;
    return minHz * powf(ratio, norm);
}

float allpassCoeff(float fc)
{
    const float g = tanf(kPi * fc / kFs);
    return (1.0f - g) / (1.0f + g);
}

struct VoiceParams
{
    float feedback;
    float sweepMinHz;
    float sweepMaxHz;
    float dryMix;
    float wetMix;
};

VoiceParams getVoice(bool scriptMode)
{
    // Script voice (no feedback, softer blend) intentionally keeps the gentler
    // Phase 90 script-logo feel. The block voice got a deeper feedback path
    // (0.36 → 0.58) and a more forward wet mix (0.62 → 0.72) — the previous
    // numbers produced a sweep that was audibly present but never really
    // "chewy" the way the real block-logo pedal is.
    if (scriptMode) {
        return {0.00f, 110.0f, 1450.0f, 0.78f, 0.62f};
    }

    return {0.58f, 120.0f, 1650.0f, 0.74f, 0.72f};
}

struct AllpassStage
{
    float z1L = 0.0f;
    float z1R = 0.0f;

    float processLeft(float input, float a)
    {
        const float output = -a * input + z1L;
        z1L = input + a * output;
        return output;
    }

    float processRight(float input, float a)
    {
        const float output = -a * input + z1R;
        z1R = input + a * output;
        return output;
    }

    void clear()
    {
        z1L = 0.0f;
        z1R = 0.0f;
    }
};
}

class Phase90Patch final : public Patch
{
public:
    void init() override
    {
        speed_    = 0.34f;
        bypassed_ = false;
        script_   = false;
        lfoPhase_ = 0.0f;
        feedbackL_ = 0.0f;
        feedbackR_ = 0.0f;
        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /* buf */) override
    {
        // No working buffer required.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_) {
            return;
        }

        const VoiceParams voice = getVoice(script_);
        const float speedHz = mapSpeedHz(speed_);
        const float phaseInc = speedHz / kFs;

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float lfo = triangleLfo(lfoPhase_);
            const float sweepHz = mapSweepHz(lfo, voice.sweepMinHz, voice.sweepMaxHz);
            const float a = allpassCoeff(sweepHz);

            const float dryL = left[i];
            const float dryR = right[i];

            float wetL = dryL + feedbackL_ * voice.feedback;
            float wetR = dryR + feedbackR_ * voice.feedback;

            wetL = stages_[0].processLeft(wetL, a);
            wetL = stages_[1].processLeft(wetL, a);
            wetL = stages_[2].processLeft(wetL, a);
            wetL = stages_[3].processLeft(wetL, a);

            wetR = stages_[0].processRight(wetR, a);
            wetR = stages_[1].processRight(wetR, a);
            wetR = stages_[2].processRight(wetR, a);
            wetR = stages_[3].processRight(wetR, a);

            feedbackL_ = wetL;
            feedbackR_ = wetR;

            left[i]  = clampUnit(dryL * voice.dryMix + wetL * voice.wetMix);
            right[i] = clampUnit(dryR * voice.dryMix + wetR * voice.wetMix);

            lfoPhase_ += phaseInc;
            if (lfoPhase_ >= 1.0f) {
                lfoPhase_ -= 1.0f;
            }
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.0f};  // intentionally unused
            case 1: return {0.0f, 1.0f, 0.34f}; // Speed
            case 2: return {0.0f, 1.0f, 0.34f}; // Expression mirrors Speed
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 1:
            case 2:
                speed_ = value;
                break;
            default:
                break;
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
            script_ = !script_;
            clearState();
        }
    }

    Color getStateLedColor() override
    {
        if (script_) {
            return bypassed_ ? Color::kDimWhite : Color::kBeige;
        }
        return bypassed_ ? Color::kDimBlue : Color::kBlue;
    }

private:
    void clearState()
    {
        for (auto& stage : stages_) {
            stage.clear();
        }
        feedbackL_ = 0.0f;
        feedbackR_ = 0.0f;
    }

    float speed_     = 0.34f;
    bool bypassed_   = false;
    bool script_     = false;
    float lfoPhase_  = 0.0f;
    float feedbackL_ = 0.0f;
    float feedbackR_ = 0.0f;
    AllpassStage stages_[4];
};

Patch* Patch::getInstance()
{
    static Phase90Patch instance;
    return &instance;
}
