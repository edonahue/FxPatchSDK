// Tube Screamer-inspired overdrive for Polyend Endless
//
// Primary voice:
//   TS808-inspired overdrive using a WDF-style diode-pair clipping core, controlled
//   bass trimming before clipping, and a bounded post-clip tone/output stage.
//
// Alternate voice:
//   Hold morphs into a TS9-style sibling voice with a slightly brighter, tighter
//   response while preserving the same pedal-family identity.
//
// Controls:
//   Left  knob  — Drive
//   Mid   knob  — Tone
//   Right knob  — Level (also expression pedal in this fork)
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

float clampSigned(float value, float limit)
{
    if (value < -limit) {
        return -limit;
    }
    if (value > limit) {
        return limit;
    }
    return value;
}

float clampUnit(float value)
{
    return clampSigned(value, 1.0f);
}

float softLimit(float value)
{
    const float absValue = fabsf(value);
    if (absValue <= 0.90f) {
        return value;
    }

    const float sign = value < 0.0f ? -1.0f : 1.0f;
    const float over = (absValue - 0.90f) / 0.22f;
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

float lerp(float a, float b, float mix)
{
    return a + (b - a) * mix;
}

class SmoothedValue
{
public:
    void init(float value, float timeMs)
    {
        current_ = value;
        target_  = value;
        setTimeMs(timeMs);
    }

    void setTimeMs(float timeMs)
    {
        const float samples = 0.001f * timeMs * kFs;
        coeff_ = samples <= 1.0f ? 0.0f : expf(-1.0f / samples);
    }

    void setTarget(float value)
    {
        target_ = value;
    }

    float process()
    {
        current_ = target_ + coeff_ * (current_ - target_);
        return current_;
    }

    float current() const
    {
        return current_;
    }

private:
    float current_ = 0.0f;
    float target_  = 0.0f;
    float coeff_   = 0.0f;
};

class OnePoleLowpass
{
public:
    float process(float input, float alpha)
    {
        state_ += alpha * (input - state_);
        return state_;
    }

    void reset()
    {
        state_ = 0.0f;
    }

private:
    float state_ = 0.0f;
};

class OnePoleHighpass
{
public:
    float process(float input, float alpha)
    {
        const float output = alpha * (prevOut_ + input - prevIn_);
        prevIn_  = input;
        prevOut_ = output;
        return output;
    }

    void reset()
    {
        prevIn_  = 0.0f;
        prevOut_ = 0.0f;
    }

private:
    float prevIn_  = 0.0f;
    float prevOut_ = 0.0f;
};

class WdfAntiparallelDiodePair
{
public:
    float process(float incident, float portResistance, float strength, float thermal)
    {
        const float rp      = portResistance < 0.04f ? 0.04f : portResistance;
        const float g       = strength < 0.001f ? 0.001f : strength;
        const float thermalClamped = thermal < 0.05f ? 0.05f : thermal;

        incident = clampSigned(incident, 2.6f);

        float voltage = clampSigned(lastVoltage_, 1.8f);
        for (int iter = 0; iter < 4; ++iter)
        {
            const float norm = clampSigned(voltage / thermalClamped, 11.0f);
            const float sh   = sinhf(norm);
            const float ch   = coshf(norm);
            const float f    = g * sh - (incident - voltage) / rp;
            const float df   = (g / thermalClamped) * ch + 1.0f / rp;
            voltage -= f / df;
            voltage = clampSigned(voltage, 1.8f);
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

struct VoiceParams
{
    float preHpHz;
    float splitHz;
    float portResistance;
    float diodeStrength;
    float thermal;
    float opAmpGain;
    float edgeDrive;
    float toneBaseHz;
    float toneRangeHz;
    float brightMix;
    float cleanKeep;
    float outputTrim;
};

VoiceParams getTs808Voice()
{
    return {
        138.0f, // preHpHz
        710.0f, // splitHz
        0.29f,  // portResistance
        0.14f,  // diodeStrength
        0.14f,  // thermal
        4.45f,  // opAmpGain
        2.35f,  // edgeDrive
        1180.0f,// toneBaseHz
        4700.0f,// toneRangeHz
        1.04f,  // brightMix
        0.18f,  // cleanKeep
        1.10f   // outputTrim
    };
}

VoiceParams getTs9Voice()
{
    return {
        152.0f, // preHpHz
        770.0f, // splitHz
        0.26f,  // portResistance
        0.16f,  // diodeStrength
        0.13f,  // thermal
        4.90f,  // opAmpGain
        2.60f,  // edgeDrive
        1380.0f,// toneBaseHz
        5600.0f,// toneRangeHz
        1.18f,  // brightMix
        0.14f,  // cleanKeep
        1.14f   // outputTrim
    };
}
} // namespace

class TubeScreamerWdfPatch final : public Patch
{
public:
    void init() override
    {
        drive_.init(0.52f, 28.0f);
        tone_.init(0.50f, 24.0f);
        level_.init(0.58f, 18.0f);
        ts9Morph_.init(0.0f, 30.0f);

        bypassed_ = false;
        ts9Mode_  = false;

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

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float driveValue = clamp01(drive_.process());
            const float toneValue  = clamp01(tone_.process());
            const float levelValue = clamp01(level_.process());
            const float ts9Morph   = clamp01(ts9Morph_.process());

            const VoiceParams a = getTs808Voice();
            const VoiceParams b = getTs9Voice();

            const float driveCurve = driveValue * driveValue *
                                     (0.40f + 0.60f * driveValue);
            const float toneCurve  = toneValue * toneValue * (0.28f + 0.72f * toneValue);
            const float levelCurve = levelValue * levelValue *
                                     (0.35f + 0.65f * levelValue);

            const float preHpAlpha = hpCoeff(lerp(a.preHpHz, b.preHpHz, ts9Morph) +
                                             42.0f * driveCurve);
            const float splitAlpha = lpCoeff(lerp(a.splitHz, b.splitHz, ts9Morph));
            const float toneAlpha  = lpCoeff(lerp(a.toneBaseHz, b.toneBaseHz, ts9Morph) +
                                             lerp(a.toneRangeHz, b.toneRangeHz, ts9Morph) * toneCurve);

            const float portResistance = lerp(a.portResistance, b.portResistance, ts9Morph) -
                                         0.05f * driveCurve;
            const float diodeStrength  = lerp(a.diodeStrength, b.diodeStrength, ts9Morph) *
                                         (0.78f + 1.72f * driveCurve);
            const float thermal        = lerp(a.thermal, b.thermal, ts9Morph);
            const float opAmpGain      = lerp(a.opAmpGain, b.opAmpGain, ts9Morph) *
                                         (0.58f + 1.95f * driveCurve);
            const float edgeDrive      = lerp(a.edgeDrive, b.edgeDrive, ts9Morph) *
                                         (0.86f + 0.54f * driveCurve);
            const float brightMix      = lerp(a.brightMix, b.brightMix, ts9Morph);
            const float cleanKeep      = lerp(a.cleanKeep, b.cleanKeep, ts9Morph) *
                                         (0.96f - 0.82f * driveCurve);
            const float outputTrim     =
              (0.12f + 1.02f * levelCurve) *
              (lerp(a.outputTrim, b.outputTrim, ts9Morph) - 0.04f * driveCurve);

            const float lowToneWeight  = cosf(toneValue * kHalfPi);
            const float highToneWeight = sinf(toneValue * kHalfPi);

            left[i]  = processSample(0, left[i], preHpAlpha, splitAlpha, toneAlpha,
                                     portResistance, diodeStrength, thermal, opAmpGain,
                                     driveCurve, edgeDrive, brightMix, cleanKeep, lowToneWeight,
                                     highToneWeight, outputTrim);
            right[i] = processSample(1, right[i], preHpAlpha, splitAlpha, toneAlpha,
                                     portResistance, diodeStrength, thermal, opAmpGain,
                                     driveCurve, edgeDrive, brightMix, cleanKeep, lowToneWeight,
                                     highToneWeight, outputTrim);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.52f}; // Drive
            case 1: return {0.0f, 1.0f, 0.50f}; // Tone
            case 2: return {0.0f, 1.0f, 0.58f}; // Level / expression
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: drive_.setTarget(value); break;
            case 1: tone_.setTarget(value); break;
            case 2: level_.setTarget(value); break;
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
            ts9Morph_.setTarget(ts9Mode_ ? 1.0f : 0.0f);
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
                        float portResistance,
                        float diodeStrength,
                        float thermal,
                        float opAmpGain,
                        float driveCurve,
                        float edgeDrive,
                        float brightMix,
                        float cleanKeep,
                        float lowToneWeight,
                        float highToneWeight,
                        float outputTrim)
    {
        const float conditioned = preHp_[channel].process(input, preHpAlpha);
        const float body        = splitLp_[channel].process(conditioned, splitAlpha);
        const float edge        = conditioned - body;

        const float opAmpInput =
          body * (1.06f + 0.34f * opAmpGain) + edge * edgeDrive;
        const float clipped =
          clipper_[channel].process(opAmpInput * opAmpGain, portResistance, diodeStrength, thermal);

        const float recovered =
          tanhf((clipped + edge * (0.18f + 0.26f * driveCurve) +
                 body * (0.04f + 0.04f * driveCurve)) * 1.54f);
        const float overdriveCore = clampUnit(recovered * 1.03f + body * cleanKeep);

        const float lowBranch  = toneLp_[channel].process(overdriveCore, toneAlpha);
        const float highBranch = overdriveCore - lowBranch;

        const float voiced =
          lowBranch * (0.95f * lowToneWeight) +
          highBranch * (brightMix * highToneWeight);

        return softLimit(voiced * outputTrim);
    }

    void clearState()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            preHp_[ch].reset();
            splitLp_[ch].reset();
            toneLp_[ch].reset();
            clipper_[ch].reset();
        }
    }

    SmoothedValue drive_;
    SmoothedValue tone_;
    SmoothedValue level_;
    SmoothedValue ts9Morph_;

    bool bypassed_ = false;
    bool ts9Mode_  = false;

    OnePoleHighpass preHp_[2];
    OnePoleLowpass  splitLp_[2];
    OnePoleLowpass  toneLp_[2];
    WdfAntiparallelDiodePair clipper_[2];
};

Patch* Patch::getInstance()
{
    static TubeScreamerWdfPatch instance;
    return &instance;
}
