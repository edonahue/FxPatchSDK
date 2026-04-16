// MXR Distortion+ — Polyend Endless SDK patch
//
// Endless-oriented interpretation of the MXR M104 Distortion+ topology:
//   Input → HP filter → gain → germanium-style clip → tone LP → compensated level → Output
//
// The original pedal couples a huge gain range and the bass-cut corner to the same pot.
// That pot law is a poor match for Endless knob travel, so this patch keeps the same
// broad signal flow but remaps the controls for smoother, guitar-friendly behavior.
//
// See docs/mxr-distortion-plus-circuit-analysis.md for the full circuit analysis and
// an explanation of what is preserved vs adapted from the hardware.
//
// Controls:
//   Left  knob  — Distortion (smooth drive sweep; also tightens the low end at higher settings)
//   Mid   knob  — Tone       (guitar-voiced post-clip low-pass, dark to bright)
//   Right knob  — Level      (output level; expression pedal heel=off, toe=full)
//   Footswitch  — Bypass toggle
//   LED         — Red (active) / Dim White (bypassed)
//
// Signal chain (per sample, stereo):
//   1. 1-pole IIR HP  (drive-coupled low-end tightening)
//   2. Gain stage     (smoothed, Distortion+-style pre-clip drive)
//   3. tanhf()        (soft germanium-style clipping)
//   4. 1-pole IIR LP  (Tone knob sweeps a guitar-relevant dark/bright range)
//   5. Level scalar   (Right knob / expression pedal, with drive compensation)
//   6. tanh output soft-clip (catches the ≥1.0 outputGain at max Level without
//      hard-clipping; also adds a small "hot" character at the top of the knob)

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
}

class MxrDistortionPlus final : public Patch
{
public:
    // --- Patch interface ---

    void init() override
    {
        dist_     = 0.42f;
        tone_     = 0.55f;
        level_    = 0.65f;
        bypassed_ = false;

        // Clear filter state for both channels
        hpPrevL_ = 0.0f;  xPrevL_ = 0.0f;
        hpPrevR_ = 0.0f;  xPrevR_ = 0.0f;
        lpPrevL_ = 0.0f;
        lpPrevR_ = 0.0f;
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /* buf */) override
    {
        // No delay lines needed — all state is in scalar members.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        const int numSamples = static_cast<int>(left.size());

        if (bypassed_) {
            // Pass-through: input already in buffers (in-place), nothing to do.
            // The host writes input into left/right before calling processAudio.
            return;
        }

        // --- Compute filter coefficients once per buffer (not per sample) ---

        // Distortion keeps the Distortion+ coupling between drive and low-end tightening,
        // but uses Endless-tuned curves instead of the original pot law.
        float distClamped = clamp01(dist_);
        float toneClamped = clamp01(tone_);
        float levelClamped = clamp01(level_);

        float driveCurve = 0.08f + 0.92f * std::pow(distClamped, 1.08f);
        float gain       = 2.0f + 18.0f * driveCurve + 10.0f * driveCurve * driveCurve;
        float clipDrive  = 1.05f + 1.55f * driveCurve;
        float recoveryDrive = 1.00f + 0.92f * driveCurve;

        float fc_hp    = 35.0f + 380.0f * distClamped * distClamped;
        float alpha_hp = 1.0f / (1.0f + kTwoPi * fc_hp / kFs);

        // Move the tone sweep into a much more audible guitar range.
        float toneCurve = std::pow(toneClamped, 1.10f);
        float fc_lp     = 650.0f + 6800.0f * toneCurve;
        float omega_lp = kTwoPi * fc_lp / kFs;
        float alpha_lp = omega_lp / (1.0f + omega_lp);
        float alpha_lp_inv = 1.0f - alpha_lp;

        // Level law widened and drive-compensation softened. The earlier
        // (1.00 - 0.24·drive) compensation pulled the maximum output down to ≈0.76
        // at max Distortion — well below the real pedal's post-clip output. The
        // 0.08 coefficient keeps a tiny amount of compensation (so cranking Drive
        // doesn't jump in perceived loudness by 3+ dB) without gutting the level
        // range. The (0.10 + 1.80·levelCurve) front-end gives ≈25 dB of knob range.
        float levelCurve = levelClamped * (0.5f + 0.5f * levelClamped);
        float outputTrim = 1.00f - 0.08f * driveCurve;
        float outputGain = (0.10f + 1.80f * levelCurve) * outputTrim;

        // --- Process each sample ---
        for (int i = 0; i < numSamples; ++i) {

            // === LEFT CHANNEL ===

            // Stage 1: 1-pole high-pass filter (RC-Euler)
            //   y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            float xL   = left[i];
            float hpL  = alpha_hp * (hpPrevL_ + xL - xPrevL_);
            xPrevL_    = xL;
            hpPrevL_   = hpL;

            // Stage 2: Distortion+-style pre-clip gain
            float gainedL = hpL * gain;

            // Stage 3: Germanium-style soft clip.
            float clippedL = tanhf(gainedL * clipDrive);
            float recoveredL =
              tanhf((clippedL * recoveryDrive + gainedL * 0.07f) * (1.0f + 0.35f * driveCurve));

            // Stage 4: 1-pole low-pass filter
            float lpL  = alpha_lp * recoveredL + alpha_lp_inv * lpPrevL_;
            lpPrevL_   = lpL;

            // Stage 5: Level (Right knob / expression pedal) + output soft-clip.
            // The widened outputGain can exceed 1.0 at max Level; tanh saturates
            // softly near the rails instead of hard-clipping at the DAC, which
            // also gives a small additional "hot into the amp" character at the
            // top of the knob rather than a shelf.
            left[i] = tanhf(lpL * outputGain);

            // === RIGHT CHANNEL ===

            float xR   = right[i];
            float hpR  = alpha_hp * (hpPrevR_ + xR - xPrevR_);
            xPrevR_    = xR;
            hpPrevR_   = hpR;

            float gainedR  = hpR * gain;
            float clippedR = tanhf(gainedR * clipDrive);
            float recoveredR =
              tanhf((clippedR * recoveryDrive + gainedR * 0.07f) * (1.0f + 0.35f * driveCurve));

            float lpR  = alpha_lp * recoveredR + alpha_lp_inv * lpPrevR_;
            lpPrevR_   = lpR;

            right[i] = tanhf(lpR * outputGain);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return { 0.0f, 1.0f, 0.42f }; // Distortion
            case 1: return { 0.0f, 1.0f, 0.55f }; // Tone
            case 2: return { 0.0f, 1.0f, 0.65f }; // Level / expression
            default: return { 0.0f, 1.0f, 0.5f };
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: dist_  = value; break;  // Left  — Distortion
            case 1: tone_  = value; break;  // Mid   — Tone
            case 2: level_ = value; break;  // Right — Level (also expression pedal)
            default: break;
        }
    }

    void handleAction(int actionId) override
    {
        // Left footswitch press: toggle bypass
        if (actionId == static_cast<int>(endless::ActionId::kLeftFootSwitchPress)) {
            bypassed_ = !bypassed_;
            // Clear filter state on bypass-off to avoid pops from stale state.
            if (!bypassed_) {
                hpPrevL_ = 0.0f; xPrevL_ = 0.0f;
                hpPrevR_ = 0.0f; xPrevR_ = 0.0f;
                lpPrevL_ = 0.0f;
                lpPrevR_ = 0.0f;
            }
        }
    }

    Color getStateLedColor() override
    {
        // Red = active (distortion engaged), Dim White = bypassed
        return bypassed_ ? Color::kDimWhite : Color::kRed;
    }

private:
    // Knob positions (0.0–1.0)
    float dist_    = 0.5f;
    float tone_    = 0.5f;
    float level_   = 0.5f;

    bool bypassed_ = false;

    // HP filter state (one set per channel)
    float hpPrevL_ = 0.0f;   // y[n-1] for left HP
    float xPrevL_  = 0.0f;   // x[n-1] for left HP
    float hpPrevR_ = 0.0f;   // y[n-1] for right HP
    float xPrevR_  = 0.0f;   // x[n-1] for right HP

    // LP filter state (one set per channel)
    float lpPrevL_ = 0.0f;   // y[n-1] for left LP
    float lpPrevR_ = 0.0f;   // y[n-1] for right LP
};

// C ABI singleton required by PatchCppWrapper.cpp
Patch* Patch::getInstance()
{
    static MxrDistortionPlus instance;
    return &instance;
}
