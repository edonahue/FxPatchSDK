// Freeverb-based Stereo Reverb
//
// Source: andybalham/FxPatchSDK
//         https://github.com/andybalham/FxPatchSDK
//         MIT License
//
// This file is adapted from source/effects/Reverb.h in andybalham's fork.
// It compiles against the stock polyend/FxPatchSDK with no additional dependencies
// (only #include "../../source/Patch.h" is needed).
//
// Simulates audio bouncing around a physical space using the classic
// Schroeder/Freeverb algorithm — 4 parallel feedback comb filters followed
// by 2 series all-pass filters. Left and right channels use slightly different
// delay lengths for stereo width.
//
// Signal flow:
//                      ┌─ Comb 1 ─┐
//  input ──────────────┤─ Comb 2 ─├─── sum ──► AllPass 1 ──► AllPass 2 ──► output
//  (mono mix)          ├─ Comb 3 ─┤
//                      └─ Comb 4 ─┘
//
// All delay lines (~50 KB total) are stored in the working buffer (external RAM).
//
// Controls:
//   Left knob  (0): Room Size — larger = longer reverb tail
//   Mid knob   (1): Damping   — 0=bright (highs linger), 1=dark (highs fade fast)
//   Right knob (2): Mix       — dry/wet blend (0=dry, 0.33=typical, 1=full wet)
//
// Footswitch:
//   Press or Hold: Toggle bypass
//
// LED: kLightBlueColor = active  |  kDimWhite = bypassed

#include "../../source/Patch.h"
#include <algorithm>

class Reverb : public Patch
{
  public:
    static constexpr int kNumCombs   = 4;
    static constexpr int kNumAllpass = 2;

    // Comb filter delay lengths in samples at 48 kHz
    // Based on Freeverb tuning (originally 44.1 kHz), scaled up by 48/44.1.
    // Left and right use slightly different lengths for stereo width.
    static constexpr int kCombSizesL[kNumCombs]    = {1214, 1292, 1388, 1474};
    static constexpr int kCombSizesR[kNumCombs]    = {1237, 1315, 1411, 1497};
    static constexpr int kAllpassSizes[kNumAllpass] = {245, 605};

    void init() override
    {
        roomSize = 0.6f;
        damping  = 0.5f;
        mix      = 0.33f;
        bypassed = false;

        for (int i = 0; i < kNumCombs; ++i)
        {
            combBufL[i] = combBufR[i] = nullptr;
            combPosL[i] = combPosR[i] = 0;
            combLpL[i]  = combLpR[i]  = 0.0f;
        }
        for (int i = 0; i < kNumAllpass; ++i)
        {
            apBufL[i] = apBufR[i] = nullptr;
            apPosL[i] = apPosR[i] = 0;
        }
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        float* ptr = buffer.data();

        // Allocate and zero-init each comb filter delay line for left channel
        for (int i = 0; i < kNumCombs; ++i)
        {
            combBufL[i] = ptr;
            std::fill(ptr, ptr + kCombSizesL[i], 0.0f);
            ptr += kCombSizesL[i];
        }
        // All-pass delay lines for left channel
        for (int i = 0; i < kNumAllpass; ++i)
        {
            apBufL[i] = ptr;
            std::fill(ptr, ptr + kAllpassSizes[i], 0.0f);
            ptr += kAllpassSizes[i];
        }
        // Right channel (different comb sizes → stereo width)
        for (int i = 0; i < kNumCombs; ++i)
        {
            combBufR[i] = ptr;
            std::fill(ptr, ptr + kCombSizesR[i], 0.0f);
            ptr += kCombSizesR[i];
        }
        for (int i = 0; i < kNumAllpass; ++i)
        {
            apBufR[i] = ptr;
            std::fill(ptr, ptr + kAllpassSizes[i], 0.0f);
            ptr += kAllpassSizes[i];
        }
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (combBufL[0] == nullptr)
            return;

        // Map room size 0–1 to comb feedback 0.5–0.95
        float fb = 0.5f + roomSize * 0.45f;

        // Map damping 0–1 to LP coefficient 0–0.9
        // Higher damping = slower LP tracking = highs fade faster
        float damp = damping * 0.9f;

        // 4 comb filters sum their outputs — scale down to prevent clipping
        const float wetGain = 0.2f;

        for (size_t i = 0; i < left.size(); ++i)
        {
            float dryL = left[i];
            float dryR = right[i];

            if (bypassed)
                continue;

            // Stereo input mixed to mono; gain 0.5 prevents level doubling
            float inputMono = (dryL + dryR) * 0.5f;

            // Run all comb filters in parallel
            float outL = 0.0f;
            float outR = 0.0f;
            for (int c = 0; c < kNumCombs; ++c)
            {
                outL += processComb(combBufL[c], kCombSizesL[c], combPosL[c], combLpL[c],
                                    inputMono, fb, damp);
                outR += processComb(combBufR[c], kCombSizesR[c], combPosR[c], combLpR[c],
                                    inputMono, fb, damp);
            }
            outL *= wetGain;
            outR *= wetGain;

            // Run all-pass filters in series to increase echo density
            for (int a = 0; a < kNumAllpass; ++a)
            {
                outL = processAllpass(apBufL[a], kAllpassSizes[a], apPosL[a], outL);
                outR = processAllpass(apBufR[a], kAllpassSizes[a], apPosR[a], outR);
            }

            // Dry/wet blend
            left[i]  = dryL * (1.0f - mix) + outL * mix;
            right[i] = dryR * (1.0f - mix) + outR * mix;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
        case endless::ParamId::kParamLeft:  return {0.0f, 1.0f, 0.6f};   // Room Size
        case endless::ParamId::kParamMid:   return {0.0f, 1.0f, 0.5f};   // Damping
        case endless::ParamId::kParamRight: return {0.0f, 1.0f, 0.33f};  // Mix
        default:                            return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (static_cast<endless::ParamId>(paramIdx))
        {
        case endless::ParamId::kParamLeft:  roomSize = value; break;
        case endless::ParamId::kParamMid:   damping  = value; break;
        case endless::ParamId::kParamRight: mix      = value; break;
        default: break;
        }
    }

    void handleAction(int /* idx */) override { bypassed = !bypassed; }

    Color getStateLedColor() override
    {
        return bypassed ? Color::kDimWhite : Color::kLightBlueColor;
    }

  private:
    float roomSize = 0.6f;
    float damping  = 0.5f;
    float mix      = 0.33f;
    bool  bypassed = false;

    // Pointers into working buffer — set up in setWorkingBuffer()
    float* combBufL[kNumCombs]   = {};
    float* combBufR[kNumCombs]   = {};
    float* apBufL[kNumAllpass]   = {};
    float* apBufR[kNumAllpass]   = {};

    // Read/write positions for each delay line
    int combPosL[kNumCombs]   = {};
    int combPosR[kNumCombs]   = {};
    int apPosL[kNumAllpass]   = {};
    int apPosR[kNumAllpass]   = {};

    // Low-pass filter state inside each comb's feedback path
    float combLpL[kNumCombs] = {};
    float combLpR[kNumCombs] = {};

    /**
     * Feedback Comb Filter — processes one sample.
     *
     * Reads the oldest sample, passes it through a 1-pole LP filter to simulate
     * damping, then writes input + damped_feedback back into the same position.
     *
     *   lpState "chases" output but can't keep up with fast (high-freq) changes —
     *   that lag IS the damping: highs decay faster than lows.
     */
    static float processComb(float* buf, int size, int& pos, float& lpState,
                              float input, float fb, float damp)
    {
        float output = buf[pos];
        lpState = output * (1.0f - damp) + lpState * damp;
        buf[pos] = input + lpState * fb;
        if (++pos >= size) pos = 0;
        return output;
    }

    /**
     * All-Pass Filter — processes one sample.
     *
     * Classic Schroeder all-pass: flat frequency response, non-flat phase.
     * Used purely to increase echo density without colouring the sound.
     *
     *   output = -input + buf[pos];  buf[pos] = input + buf[pos] * g
     */
    static float processAllpass(float* buf, int size, int& pos, float input)
    {
        constexpr float g = 0.5f; // Standard Freeverb all-pass gain
        float bufOut = buf[pos];
        buf[pos] = input + bufOut * g;
        if (++pos >= size) pos = 0;
        return bufOut - input;
    }
};

static Reverb patch;

Patch* Patch::getInstance()
{
    return &patch;
}
