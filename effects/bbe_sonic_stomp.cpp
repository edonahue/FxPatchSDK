// BBE Sonic Stomp / Aion Lumin-inspired enhancer for Polyend Endless
//
// Behavioral target:
//   - broad-band "sonic enhancer" feel rather than literal chip emulation
//   - guitar-oriented tuning inspired by the 3-knob Lumin clone interpretation
//   - three controls: Contour, Process, Midrange
//
// DSP approach:
//   1. Split the signal into low / mid / high bands with two complementary 1-pole LP stages
//   2. Delay the bands by different amounts to create frequency-dependent time alignment
//   3. Add the delayed-band deltas back to a common dry path
//   4. Optionally apply a subtle stereo doubler on footswitch hold
//
// Controls:
//   Left knob  — Contour   (low-band correction amount)
//   Mid knob   — Process   (high-band correction amount)
//   Right knob — Midrange  (bandpass correction amount)
//
// Expression pedal:
//   In this fork, expression is globally routed to param 2, so this patch treats the pedal
//   as a Midrange controller. Heel down = less mid correction, toe down = more mid correction.
//   When expression is connected, the physical Right knob is ignored by the firmware.
//
// Footswitch:
//   Press — bypass toggle
//   Hold  — toggle doubler mode (stretch goal, off by default)
//
// LED:
//   LightGreen = enhancer only, active
//   LightBlue  = enhancer + doubler, active
//   DimGreen   = enhancer only, bypassed
//   DimBlue    = enhancer + doubler, bypassed

#include "../source/Patch.h"

#include <cmath>
#include <cstddef>

namespace {
constexpr float kTwoPi = 6.283185307f;

// Guitar-oriented tuning chosen from the stock Sonic Stomp anchors (50 Hz / 10 kHz)
// and Aion Lumin's public recommendations for more guitar-friendly switch settings.
constexpr float kLowCrossoverHz  = 80.0f;
constexpr float kHighCrossoverHz = 5600.0f;

constexpr int kEnhanceDelayLen = 192;
constexpr int kBaseDelay       = 32;

// Relative delays around the shared dry path.
constexpr int kLowMaxExtraDelay  = 64;  // 32 -> 96 samples total
constexpr int kHighMaxLead       = 8;   // 32 -> 24 samples total
constexpr int kMidMaxOffset      = 12;  // 20 -> 44 samples total

// Doubler uses a small portion of the working buffer.
constexpr int kDoublerLen = 1536;  // 32 ms @ 48 kHz
constexpr float kDoublerDelayL = 620.0f;  // ~12.9 ms
constexpr float kDoublerDelayR = 870.0f;  // ~18.1 ms
constexpr float kDoublerModL   = 35.0f;   // ~0.7 ms
constexpr float kDoublerModR   = 48.0f;   // ~1.0 ms
constexpr float kDoublerRateL  = 0.37f;
constexpr float kDoublerRateR  = 0.53f;
constexpr float kDoublerToneHz = 3200.0f;

inline float clampUnit(float x)
{
    if (x > 1.0f)
        return 1.0f;
    if (x < -1.0f)
        return -1.0f;
    return x;
}

inline int clampInt(int x, int lo, int hi)
{
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

inline float lpCoeff(float fc)
{
    const float omega = kTwoPi * fc / static_cast<float>(Patch::kSampleRate);
    return omega / (1.0f + omega);
}
} // namespace

class SonicStompEnhancer final : public Patch
{
  public:
    void init() override
    {
        contour_ = 0.45f;
        process_ = 0.45f;
        mid_     = 0.5f;

        bypassed_       = false;
        doublerEnabled_ = false;

        clearEnhancerState();
        clearDoublerState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
    {
        doublerDelayL_ = buf.data();
        doublerDelayR_ = buf.data() + kDoublerLen;

        for (int i = 0; i < kDoublerLen; ++i)
        {
            doublerDelayL_[i] = 0.0f;
            doublerDelayR_[i] = 0.0f;
        }

        clearDoublerState();
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_)
            return;

        const float lowAlpha  = lpCoeff(kLowCrossoverHz);
        const float highAlpha = lpCoeff(kHighCrossoverHz);

        const int lowDelay =
          clampInt(kBaseDelay + static_cast<int>(contour_ * static_cast<float>(kLowMaxExtraDelay) + 0.5f),
                   kBaseDelay,
                   kEnhanceDelayLen - 1);
        const int highDelay =
          clampInt(kBaseDelay - static_cast<int>(process_ * static_cast<float>(kHighMaxLead) + 0.5f),
                   0,
                   kBaseDelay);
        const int midDelay =
          clampInt(kBaseDelay + static_cast<int>((0.5f - mid_) * static_cast<float>(kMidMaxOffset) * 2.0f),
                   0,
                   kEnhanceDelayLen - 1);

        // Blend caps raised from 0.85/0.75/0.80 → 1.10/1.00/1.05 (≈ +2 dB each).
        // The earlier caps meant the knobs were effectively always "turned down"
        // even at max — the enhancer reads audibly on transients but barely
        // shifts perceived tone on sustained chords. The clampUnit at the output
        // still protects against peaks if all three are cranked simultaneously.
        const float contourBlend = 1.10f * contour_;
        const float processBlend = 1.00f * process_;
        const float midBlend     = 1.05f * mid_;

        const float doublerToneAlpha = lpCoeff(kDoublerToneHz);
        const float phaseIncL = kDoublerRateL / static_cast<float>(kSampleRate);
        const float phaseIncR = kDoublerRateR / static_cast<float>(kSampleRate);

        for (size_t i = 0; i < left.size(); ++i)
        {
            const float enhancedL =
              processEnhancerSample(0, left[i], lowAlpha, highAlpha, lowDelay, midDelay, highDelay,
                                    contourBlend, midBlend, processBlend);
            const float enhancedR =
              processEnhancerSample(1, right[i], lowAlpha, highAlpha, lowDelay, midDelay, highDelay,
                                    contourBlend, midBlend, processBlend);

            float outL = enhancedL;
            float outR = enhancedR;

            if (doublerEnabled_ && doublerDelayL_ != nullptr && doublerDelayR_ != nullptr)
            {
                doublerDelayL_[doublerWrite_] = enhancedL;
                doublerDelayR_[doublerWrite_] = enhancedR;

                const float delayL =
                  kDoublerDelayL + kDoublerModL * sinf(doublerPhaseL_ * kTwoPi);
                const float delayR =
                  kDoublerDelayR + kDoublerModR * sinf(doublerPhaseR_ * kTwoPi);

                const float voiceL = readTapInterpolated(doublerDelayL_, kDoublerLen, doublerWrite_, delayL);
                const float voiceR = readTapInterpolated(doublerDelayR_, kDoublerLen, doublerWrite_, delayR);

                // Light low-pass smoothing makes the overdubs feel less like slapback.
                doublerToneL_ += doublerToneAlpha * (voiceL - doublerToneL_);
                doublerToneR_ += doublerToneAlpha * (voiceR - doublerToneR_);

                outL = enhancedL * 0.88f + doublerToneL_ * 0.16f + doublerToneR_ * 0.08f;
                outR = enhancedR * 0.88f + doublerToneR_ * 0.16f + doublerToneL_ * 0.08f;

                if (++doublerWrite_ >= kDoublerLen)
                    doublerWrite_ = 0;

                doublerPhaseL_ += phaseIncL;
                if (doublerPhaseL_ >= 1.0f)
                    doublerPhaseL_ -= 1.0f;

                doublerPhaseR_ += phaseIncR;
                if (doublerPhaseR_ >= 1.0f)
                    doublerPhaseR_ -= 1.0f;
            }

            left[i]  = clampUnit(outL);
            right[i] = clampUnit(outR);

            if (++enhanceWrite_ >= kEnhanceDelayLen)
                enhanceWrite_ = 0;
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx)
        {
        case 0: return {0.0f, 1.0f, 0.45f}; // Contour
        case 1: return {0.0f, 1.0f, 0.45f}; // Process
        case 2: return {0.0f, 1.0f, 0.5f};  // Midrange (also expression in this fork)
        default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx)
        {
        case 0: contour_ = value; break;
        case 1: process_ = value; break;
        case 2: mid_     = value; break;    // Right knob or expression pedal
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
                clearEnhancerState();
                clearDoublerState();
            }
        }
        else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold))
        {
            doublerEnabled_ = !doublerEnabled_;
            clearDoublerState();
        }
    }

    Color getStateLedColor() override
    {
        if (doublerEnabled_)
            return bypassed_ ? Color::kDimBlue : Color::kLightBlueColor;

        return bypassed_ ? Color::kDimGreen : Color::kLightGreen;
    }

  private:
    float processEnhancerSample(int channel,
                                float input,
                                float lowAlpha,
                                float highAlpha,
                                int lowDelay,
                                int midDelay,
                                int highDelay,
                                float contourBlend,
                                float midBlend,
                                float processBlend)
    {
        // Complementary split:
        //   low  = LP(low_fc)
        //   high = input - LP(high_fc)
        //   mid  = LP(high_fc) - LP(low_fc)
        upperLp_[channel] += highAlpha * (input - upperLp_[channel]);
        lowLp_[channel] += lowAlpha * (input - lowLp_[channel]);

        const float lowBand  = lowLp_[channel];
        const float midBand  = upperLp_[channel] - lowLp_[channel];
        const float highBand = input - upperLp_[channel];

        programDelay_[channel][enhanceWrite_] = input;
        lowDelay_[channel][enhanceWrite_]     = lowBand;
        midDelay_[channel][enhanceWrite_]     = midBand;
        highDelay_[channel][enhanceWrite_]    = highBand;

        const float dryBase =
          readTap(programDelay_[channel], kEnhanceDelayLen, enhanceWrite_, kBaseDelay);
        const float lowBase =
          readTap(lowDelay_[channel], kEnhanceDelayLen, enhanceWrite_, kBaseDelay);
        const float midBase =
          readTap(midDelay_[channel], kEnhanceDelayLen, enhanceWrite_, kBaseDelay);
        const float highBase =
          readTap(highDelay_[channel], kEnhanceDelayLen, enhanceWrite_, kBaseDelay);

        const float lowShift =
          readTap(lowDelay_[channel], kEnhanceDelayLen, enhanceWrite_, lowDelay);
        const float midShift =
          readTap(midDelay_[channel], kEnhanceDelayLen, enhanceWrite_, midDelay);
        const float highShift =
          readTap(highDelay_[channel], kEnhanceDelayLen, enhanceWrite_, highDelay);

        const float correction = contourBlend * (lowShift - lowBase)
                                 + midBlend * (midShift - midBase)
                                 + processBlend * (highShift - highBase);

        return dryBase + correction;
    }

    void clearEnhancerState()
    {
        enhanceWrite_ = 0;

        lowLp_[0] = lowLp_[1] = 0.0f;
        upperLp_[0] = upperLp_[1] = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kEnhanceDelayLen; ++i)
            {
                programDelay_[ch][i] = 0.0f;
                lowDelay_[ch][i]     = 0.0f;
                midDelay_[ch][i]     = 0.0f;
                highDelay_[ch][i]    = 0.0f;
            }
        }
    }

    void clearDoublerState()
    {
        doublerWrite_  = 0;
        doublerPhaseL_ = 0.0f;
        doublerPhaseR_ = 0.31f;
        doublerToneL_  = 0.0f;
        doublerToneR_  = 0.0f;

        if (doublerDelayL_ != nullptr && doublerDelayR_ != nullptr)
        {
            for (int i = 0; i < kDoublerLen; ++i)
            {
                doublerDelayL_[i] = 0.0f;
                doublerDelayR_[i] = 0.0f;
            }
        }
    }

    static float readTap(const float* buffer, int length, int writeIdx, int delaySamples)
    {
        int idx = writeIdx - delaySamples;
        while (idx < 0)
            idx += length;
        return buffer[idx % length];
    }

    static float readTapInterpolated(const float* buffer,
                                     int length,
                                     int writeIdx,
                                     float delaySamples)
    {
        float readPos = static_cast<float>(writeIdx) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(length);

        const int idx0 = static_cast<int>(readPos) % length;
        const int idx1 = (idx0 + 1) % length;
        const float frac = readPos - static_cast<float>(idx0);
        return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
    }

    float contour_ = 0.45f;
    float process_ = 0.45f;
    float mid_     = 0.5f;

    bool bypassed_       = false;
    bool doublerEnabled_ = false;

    float lowLp_[2]   = {0.0f, 0.0f};
    float upperLp_[2] = {0.0f, 0.0f};

    float programDelay_[2][kEnhanceDelayLen] = {};
    float lowDelay_[2][kEnhanceDelayLen]     = {};
    float midDelay_[2][kEnhanceDelayLen]     = {};
    float highDelay_[2][kEnhanceDelayLen]    = {};
    int enhanceWrite_ = 0;

    float* doublerDelayL_ = nullptr;
    float* doublerDelayR_ = nullptr;
    int doublerWrite_ = 0;
    float doublerPhaseL_ = 0.0f;
    float doublerPhaseR_ = 0.31f;
    float doublerToneL_ = 0.0f;
    float doublerToneR_ = 0.0f;
};

Patch* Patch::getInstance()
{
    static SonicStompEnhancer instance;
    return &instance;
}
