// Back Talk Reverse Delay — Polyend Endless SDK patch
//
// Inspired by the Danelectro Back Talk reverse delay as documented by ElectroSmash.
// The original pedal is a pure digital reverse delay with three controls:
// Mix, Speed, and Repetitions.
//
// This Endless version keeps that core control story, then adds one Endless-native
// performance idea: a hold-toggle Texture mode that layers an older reverse slice
// under the main slice for a smoother, dreamier bloom.
//
// Controls:
//   Left  knob  — Speed       (reverse window / delay time)
//   Mid   knob  — Repetitions (feedback / repeats)
//   Right knob  — Mix         (dry/wet blend; expression pedal controls this)
//   Footswitch  — Press: bypass toggle, Hold: Texture mode toggle
//   LED         — LightBlue/DimBlue normal, Magenta/DimCyan texture mode
//
// Signal flow:
//   input -> working-buffer circular capture -> reverse chunk playback -> feedback -> mix

#include "../source/Patch.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kHalfPi = 1.57079632679f;
constexpr int   kDelayLen = 131072;               // 2.73 s per channel @ 48 kHz
constexpr int   kDelayMask = kDelayLen - 1;
constexpr float kMinChunkSec = 0.075f;
constexpr float kMaxChunkSec = 1.20f;
constexpr float kFeedbackFilterAlpha = 0.18f;
constexpr int   kMaxFadeSamples = 512;
constexpr int   kMinFadeSamples = 64;

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

int wrapIndex(int idx)
{
    return static_cast<int>(static_cast<unsigned int>(idx) & static_cast<unsigned int>(kDelayMask));
}

int chunkSamplesFromSpeed(float speed)
{
    const float ratio = kMaxChunkSec / kMinChunkSec;
    const float secs  = kMinChunkSec * std::pow(ratio, clamp01(speed));
    const int samples = static_cast<int>(secs * static_cast<float>(Patch::kSampleRate) + 0.5f);
    const int maxSafe = (kDelayLen / 2) - 256;
    return std::clamp(samples, 1024, maxSafe);
}

float equalPowerDry(float mix)
{
    return std::cos(clamp01(mix) * kHalfPi);
}

float equalPowerWet(float mix)
{
    // Earlier releases capped the wet leg at 0.92, so turning Mix to max still
    // left the dry signal audible at the equivalent of ≈ −0.7 dB under the
    // reversed wet. A real Back Talk can go completely wet. The output
    // clampUnit downstream catches any peak excursions.
    return std::sin(clamp01(mix) * kHalfPi);
}

float feedbackGainFromRepetitions(float repetitions)
{
    return 0.965f * std::pow(clamp01(repetitions), 1.7f);
}

float chunkEnvelope(int playPos, int chunkLen)
{
    const int fadeSamples = std::clamp(chunkLen / 8, kMinFadeSamples, kMaxFadeSamples);
    const int samplesToEnd = chunkLen - 1 - playPos;

    float env = 1.0f;
    if (playPos < fadeSamples) {
        env = static_cast<float>(playPos) / static_cast<float>(fadeSamples);
    }
    if (samplesToEnd < fadeSamples) {
        env = std::min(env, static_cast<float>(samplesToEnd) / static_cast<float>(fadeSamples));
    }
    if (env < 0.0f) {
        env = 0.0f;
    }
    return env;
}
}

class BackTalkReverseDelay final : public Patch
{
public:
    void init() override
    {
        speed_        = 0.42f;
        repetitions_  = 0.32f;
        mix_          = 0.38f;
        bypassed_     = false;
        textureMode_  = false;

        resetDelayEngine();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
    {
        delayL_ = buf.data();
        delayR_ = buf.data() + kDelayLen;
        clearDelayLines();
        resetDelayEngine();
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (!delayL_ || !delayR_) {
            return;
        }

        if (bypassed_) {
            return;
        }

        const int targetChunkLen = chunkSamplesFromSpeed(speed_);
        const float feedbackGain = feedbackGainFromRepetitions(repetitions_);
        const float dryGain      = equalPowerDry(mix_);
        const float wetGain      = equalPowerWet(mix_);
        const int textureOffset  = targetChunkLen / 2;

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];

            if (!chunkActive_ && capturedSamples_ >= static_cast<unsigned int>(targetChunkLen))
            {
                startNewChunk(targetChunkLen);
            }

            float wetCoreL = 0.0f;
            float wetCoreR = 0.0f;

            if (chunkActive_)
            {
                const float env = chunkEnvelope(playPos_, activeChunkLen_);

                const float primaryL = readReverseSample(delayL_, chunkEnd_, playPos_, 0) * env;
                const float primaryR = readReverseSample(delayR_, chunkEnd_, playPos_, 0) * env;

                wetCoreL = primaryL;
                wetCoreR = primaryR;

                if (textureMode_ && capturedSamples_ >= static_cast<unsigned int>(activeChunkLen_ + textureOffset))
                {
                    const float secondaryL = readReverseSample(delayL_, chunkEnd_, playPos_, textureOffset) * env;
                    const float secondaryR = readReverseSample(delayR_, chunkEnd_, playPos_, textureOffset) * env;

                    wetCoreL = primaryL * 0.82f + secondaryL * 0.38f;
                    wetCoreR = primaryR * 0.82f + secondaryR * 0.38f;
                }

                ++playPos_;
                if (playPos_ >= activeChunkLen_)
                {
                    chunkActive_ = false;
                    playPos_ = 0;
                }
            }

            wetCoreL = clampUnit(wetCoreL);
            wetCoreR = clampUnit(wetCoreR);

            const float outL = clampUnit(dryL * dryGain + wetCoreL * wetGain);
            const float outR = clampUnit(dryR * dryGain + wetCoreR * wetGain);

            left[i]  = outL;
            right[i] = outR;

            feedbackLpL_ += kFeedbackFilterAlpha * (wetCoreL - feedbackLpL_);
            feedbackLpR_ += kFeedbackFilterAlpha * (wetCoreR - feedbackLpR_);

            delayL_[writePos_] = clampUnit(dryL + feedbackLpL_ * feedbackGain);
            delayR_[writePos_] = clampUnit(dryR + feedbackLpR_ * feedbackGain);

            writePos_ = wrapIndex(writePos_ + 1);
            if (capturedSamples_ < static_cast<unsigned int>(kDelayLen))
            {
                ++capturedSamples_;
            }
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return { 0.0f, 1.0f, 0.42f }; // Speed
            case 1: return { 0.0f, 1.0f, 0.32f }; // Repetitions
            case 2: return { 0.0f, 1.0f, 0.38f }; // Mix / expression
            default: return { 0.0f, 1.0f, 0.5f };
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: speed_ = value; break;
            case 1: repetitions_ = value; break;
            case 2: mix_ = value; break;
            default: break;
        }
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress))
        {
            bypassed_ = !bypassed_;
            if (!bypassed_)
            {
                clearDelayLines();
                resetDelayEngine();
            }
        }
        else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold))
        {
            textureMode_ = !textureMode_;
            resetPlaybackState();
        }
    }

    Color getStateLedColor() override
    {
        if (textureMode_) {
            return bypassed_ ? Color::kDimCyan : Color::kMagenta;
        }
        return bypassed_ ? Color::kDimBlue : Color::kLightBlueColor;
    }

private:
    static float readReverseSample(const float* buffer, int chunkEnd, int playPos, int extraOffset)
    {
        return buffer[wrapIndex(chunkEnd - playPos - extraOffset)];
    }

    void startNewChunk(int chunkLen)
    {
        activeChunkLen_ = chunkLen;
        chunkEnd_ = wrapIndex(writePos_ - 1);
        playPos_ = 0;
        chunkActive_ = true;
    }

    void resetPlaybackState()
    {
        activeChunkLen_ = 0;
        chunkEnd_ = 0;
        playPos_ = 0;
        chunkActive_ = false;
        feedbackLpL_ = 0.0f;
        feedbackLpR_ = 0.0f;
    }

    void resetDelayEngine()
    {
        writePos_ = 0;
        capturedSamples_ = 0;
        resetPlaybackState();
    }

    void clearDelayLines()
    {
        if (!delayL_ || !delayR_) {
            return;
        }

        std::fill(delayL_, delayL_ + kDelayLen, 0.0f);
        std::fill(delayR_, delayR_ + kDelayLen, 0.0f);
    }

    float speed_       = 0.42f;
    float repetitions_ = 0.32f;
    float mix_         = 0.38f;

    bool bypassed_    = false;
    bool textureMode_ = false;

    float* delayL_ = nullptr;
    float* delayR_ = nullptr;

    int writePos_       = 0;
    unsigned int capturedSamples_ = 0;

    int  activeChunkLen_ = 0;
    int  chunkEnd_       = 0;
    int  playPos_        = 0;
    bool chunkActive_    = false;

    float feedbackLpL_ = 0.0f;
    float feedbackLpR_ = 0.0f;
};

Patch* Patch::getInstance()
{
    static BackTalkReverseDelay instance;
    return &instance;
}
