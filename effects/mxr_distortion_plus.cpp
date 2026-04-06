// MXR Distortion+ — Polyend Endless SDK patch
//
// Analog circuit model based on the MXR M104 Distortion+:
//   Input → HP filter → LM741 gain → 1N270 diode clip → LP tone → Level → Output
//
// See docs/mxr-distortion-plus-circuit-analysis.md for full circuit analysis.
// See docs/circuit-to-patch-conversion.md for the general DSP methodology.
//
// Controls:
//   Left  knob  — Distortion (0=clean, 1=maximum gain; also shifts HP cutoff as in hardware)
//   Mid   knob  — Tone       (0=dark/3kHz LP, 1=bright/20kHz LP)
//   Right knob  — Level      (0=silent, 1=full; expression pedal heel=0, toe=full)
//   Footswitch  — Bypass toggle
//   LED         — Red (active) / Dim White (bypassed)
//
// Signal chain (per sample, stereo):
//   1. 1-pole IIR HP  (cutoff coupled to Distortion knob, as in hardware R+C network)
//   2. Op-amp gain    (1 + 1MΩ / Ri) where Ri = 4.7kΩ + (1-dist)*1MΩ
//   3. tanhf()        (soft germanium diode clip; output bounded to (-1,+1))
//   4. 1-pole IIR LP  (Tone knob sweeps 3–20 kHz)
//   5. Level scalar   (Right knob / expression pedal)

#include "../source/Patch.h"
#include <cmath>

namespace {
    constexpr float kTwoPi    = 6.283185307f;
    constexpr float kFs       = static_cast<float>(Patch::kSampleRate);
    constexpr float kC1       = 47e-9f;   // input coupling capacitor (47 nF)
    constexpr float kR_fixed  = 4700.0f;  // fixed series resistor (4.7 kΩ)
    constexpr float kR2       = 1e6f;     // feedback resistor (1 MΩ)
    constexpr float kRV_max   = 1e6f;     // Distortion pot maximum value (1 MΩ)
}

class MxrDistortionPlus final : public Patch
{
public:
    // --- Patch interface ---

    void init() override
    {
        dist_    = 0.5f;
        tone_    = 0.5f;
        level_   = 0.5f;
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

        // Distortion knob sets the wiper position of the hardware pot.
        // RV=0 → maximum distortion (CCW), RV=kRV_max → minimum distortion (CW).
        // Note: dist_ = 1.0 means "max distortion", so RV = (1-dist_)*kRV_max.
        float RV  = (1.0f - dist_) * kRV_max;
        float Ri  = kR_fixed + RV;              // total input resistor

        // Op-amp gain: non-inverting configuration, gain = 1 + R2/Ri
        float gain = 1.0f + kR2 / Ri;           // range: ~2× (min) to ~213× (max)

        // HP cutoff: coupled to Distortion pot (same Ri as gain stage)
        float fc_hp    = 1.0f / (kTwoPi * kC1 * Ri);  // ~3.4 Hz (min) to ~720 Hz (max)
        float alpha_hp = 1.0f / (1.0f + kTwoPi * fc_hp / kFs);

        // LP cutoff: controlled independently by Tone knob (3–20 kHz)
        float fc_lp    = 3000.0f + tone_ * 17000.0f;
        float omega_lp = kTwoPi * fc_lp / kFs;
        float alpha_lp = omega_lp / (1.0f + omega_lp);
        float alpha_lp_inv = 1.0f - alpha_lp;

        // --- Process each sample ---
        for (int i = 0; i < numSamples; ++i) {

            // === LEFT CHANNEL ===

            // Stage 1: 1-pole high-pass filter (RC-Euler)
            //   y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            float xL   = left[i];
            float hpL  = alpha_hp * (hpPrevL_ + xL - xPrevL_);
            xPrevL_    = xL;
            hpPrevL_   = hpL;

            // Stage 2: Op-amp gain
            float gainedL = hpL * gain;

            // Stage 3: Germanium soft clip (1N270 diode model, k=1)
            //   tanhf(x) with k=1 matches the soft, low-threshold (~0.3V) germanium I-V curve.
            //   For silicon diodes (1N914 / DOD 250 character), use tanhf(x * 3.0f) for a
            //   sharper knee. Output bounded to (-1, +1) regardless of input level.
            float clippedL = tanhf(gainedL);

            // Stage 4: 1-pole low-pass filter (tone control + partial aliasing rolloff)
            //   y[n] = alpha * x[n] + (1-alpha) * y[n-1]
            //   Note: the LP also attenuates some aliased harmonics introduced by tanhf.
            //   For full anti-aliasing, 2x oversampling before Stage 3 would be needed.
            float lpL  = alpha_lp * clippedL + alpha_lp_inv * lpPrevL_;
            lpPrevL_   = lpL;

            // Stage 5: Level (Right knob / expression pedal)
            left[i] = lpL * level_;

            // === RIGHT CHANNEL ===

            float xR   = right[i];
            float hpR  = alpha_hp * (hpPrevR_ + xR - xPrevR_);
            xPrevR_    = xR;
            hpPrevR_   = hpR;

            float gainedR  = hpR * gain;
            float clippedR = tanhf(gainedR);

            float lpR  = alpha_lp * clippedR + alpha_lp_inv * lpPrevR_;
            lpPrevR_   = lpR;

            right[i] = lpR * level_;
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        // All params: 0.0 to 1.0, default 0.5
        (void)idx;
        return { 0.0f, 1.0f, 0.5f };
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
