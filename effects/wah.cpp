// Wah Wah — Polyend Endless SDK patch
//
// Dual-mode wah effect approximating the Dunlop Crybaby GCB-95 and Vox V847 characters.
// The expression pedal sweeps the filter frequency — heel = low (bassy), toe = high (bright).
//
// See docs/wah-build-walkthrough.md for full design decisions and circuit references.
// See docs/circuit-to-patch-conversion.md for the SVF filter primitive documentation.
//
// Controls:
//   Left  knob  — Mix       (0 = dry passthrough, 1 = full wet wah; default 1.0)
//   Mid   knob  — Q/Resonance (1.0–10.0; default Q≈7 for Crybaby, Q≈4.5 for Vox)
//   Right knob  — Wah position (heel=350 Hz, toe=2500/2200 Hz; controlled by expression pedal)
//   Footswitch press — Bypass toggle
//   Footswitch hold  — Toggle mode: Crybaby ↔ Vox
//   LED:
//     Red        = Crybaby, active
//     LightYellow = Vox, active
//     DarkRed    = Crybaby, bypassed
//     DimYellow  = Vox, bypassed
//
// Signal chain (per sample, stereo):
//   RBJ bandpass biquad → resonant peak boost → tanh "op-amp growl"
//   → equal-power dry/wet crossfade → safety soft-clip
//
// Filter: RBJ constant-peak-gain bandpass biquad (Audio EQ Cookbook), 2nd order
//   Per buffer: w0 = 2*pi*fc/fs, alpha = sin(w0)/(2*Q); b0=alpha, b2=-alpha,
//               a1=-2*cos(w0), a2=1-alpha, all divided by a0=1+alpha
//   Per sample: dsp::BandpassBiquad::process (Direct Form II Transposed)
//   Output: band * kResonanceGain (≈ +15 dB at the resonant peak, Q-independent
//   by construction — the old SVF's peak was already accidentally
//   Q-independent too, at the same magnitude; see the constant's own comment
//   below) — see the 2026-08-24 addendum in docs/wah-build-walkthrough.md for
//   why the filter itself changed: the old SVF's actual resonant peak drifted
//   up to 172.3 cents from its target at low Q.

#include "../source/Patch.h"
#include "../source/dsp/biquad.h"
#include "../source/dsp/crossfade.h"
#include "../source/dsp/filter_coeff.h"
#include <cmath>

namespace {
    constexpr float kFs = static_cast<float>(Patch::kSampleRate);

    // Frequency range — both modes share the same low end (heel position)
    constexpr float kFcMin        = 350.0f;   // Hz at heel (both modes)
    constexpr float kCrybabyFcMax = 2500.0f;  // Hz at toe — Dunlop GCB-95 character
    constexpr float kVoxFcMax     = 2200.0f;  // Hz at toe — Vox V847 character (narrower, warmer)

    // Q defaults by mode (user Q knob overrides, these are the knob-at-center starting points)
    // Crybaby: Q≈7 (knob_default=0.667), Vox: Q≈4.5 (knob_default=0.389)
    // The Q knob maps: Q = 1.0 + q_knob * 9.0 (linear, 1.0–10.0)
    // Raised from Q≈5/Q≈3 in the initial release — the earlier defaults combined
    // with a unity-peak normalization produced a filter sweep that was far more
    // subdued than the real pedals, especially into clean amps.
    constexpr float kCrybabyQKnobDefault = 0.667f;   // → Q ≈ 7.0
    constexpr float kVoxQKnobDefault     = 0.389f;   // → Q ≈ 4.5

    // Fixed resonant-peak gain applied to the bandpass output. The RBJ biquad's
    // peak is exactly 1.0x input at fc for any Q, by construction -- verified
    // to 5 decimal places in tests/dsp/biquad_test.cpp.
    //
    // The Chamberlin SVF this replaced carried its own, separate bug: its
    // in-code comment claimed "raw bandpass peak gain = Q/2", but direct
    // measurement of the actual recursion (low+=f1*band; hi=in-low-q1*band;
    // band+=f1*hi) shows the true raw peak is Q, not Q/2 -- confirmed across
    // Q=1..20, matching to 4 decimal places. Because the old bpGain formula
    // was `(2/Q)*kResonanceGain`, the wrong "/2" and the real factor of Q
    // happened to cancel exactly, so the OLD effect's actual peak gain was
    // already Q-independent in practice: 2*kResonanceGain = 5.6, not the
    // Q-dependent value its own comment described. kResonanceGain is set here
    // to that same 5.6 directly, so the new (correctly, not accidentally,
    // Q-independent) formula reproduces the actual old loudness rather than
    // the old formula's stated-but-never-true intent.
    constexpr float kResonanceGain = 5.6f;

    // Log taper ratio for frequency sweep (computed from constants, avoids runtime division)
    // fc = kFcMin * powf(fc_max / kFcMin, wahPos)
    constexpr float kCrybabyRatio = kCrybabyFcMax / kFcMin;  // 2500/350 ≈ 7.14
    constexpr float kVoxRatio     = kVoxFcMax     / kFcMin;  // 2200/350 ≈ 6.29
}

// Wah mode: each mode has different fc_max and Q character
enum class WahMode { kCrybaby, kVox };

class Wah final : public Patch
{
public:
    // -------------------------------------------------------------------------
    // Patch interface
    // -------------------------------------------------------------------------

    void init() override
    {
        mix_      = 1.0f;                    // full wet by default
        q_knob_   = kCrybabyQKnobDefault;    // Q ≈ 7 (Crybaby feel)
        wahPos_   = 0.5f;                    // mid sweep ("cocked wah" position)
        mode_     = WahMode::kCrybaby;
        bypassed_ = false;
        clearFilterState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /* buf */) override
    {
        // No delay lines needed — all state is scalar members.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        const int numSamples = static_cast<int>(left.size());

        if (bypassed_) {
            // Pure pass-through — input is already in buffers, nothing to do.
            return;
        }

        // --- Compute biquad coefficients once per buffer ---

        // Log-taper frequency sweep: fc = kFcMin * (fc_max/kFcMin)^wahPos
        // This gives perceptually even spacing: heel=350 Hz, mid≈935 Hz, toe=2500/2200 Hz.
        // powf is called once per buffer (~1000 samples) — negligible CPU cost.
        const float ratio = (mode_ == WahMode::kCrybaby) ? kCrybabyRatio : kVoxRatio;
        const float fc    = kFcMin * powf(ratio, wahPos_);

        // Q knob: Q = 1.0 + q_knob_ * 9.0  (linear, range 1.0–10.0)
        const float q_ = 1.0f + q_knob_ * 9.0f;

        // RBJ constant-peak-gain bandpass coefficients (dsp::rbjBandpassCoeffs,
        // source/dsp/filter_coeff.h — shared with funk_machine_envelope_filter.cpp
        // and harmonica.cpp). One more cosf than the Chamberlin form this
        // replaced; still called once per buffer, so the added cost is negligible.
        const dsp::BiquadCoeffs coeffs = dsp::rbjBandpassCoeffs(fc, q_, kFs);

        // Bandpass output scaling: the RBJ biquad's peak is exactly 1.0x input
        // at fc for any Q, so kResonanceGain is the whole story here -- no
        // per-Q correction needed (see the constant's own comment above).
        const float bpGain = kResonanceGain;

        // Equal-power dry/wet crossfade. Linear blends have a −3 dB dip at mix=0.5;
        // equal-power keeps perceived loudness constant across the knob sweep, which
        // lets full-wet actually read as "more wah" rather than just "same level but
        // filtered." dsp::equalPower clamps its input internally, so mix_ is passed
        // straight through rather than clamped again here first.
        const auto  mixGains = dsp::equalPower(mix_);
        const float dry      = mixGains.dry;
        const float wet      = mixGains.wet;

        // Op-amp "growl" drive. Real wah circuits feed the peaking filter into a
        // transistor/op-amp stage that softly clips at the top of the swept peak —
        // it's what gives the pedal its vocal bite rather than a sterile filter sweep.
        // tanh(x*1.2)/1.2 gives gentle saturation starting around ±0.8 without
        // noticeably changing the fundamental at normal levels.
        constexpr float kGrowlDrive = 1.2f;
        constexpr float kGrowlInv   = 1.0f / 1.2f;

        // Final output safety soft-clipper. The resonant peak sits at ~×2.8 (+9 dB),
        // and transients on a strong input can briefly push beyond ±1 even after the
        // growl stage. tanh(out*0.85)/0.85 keeps the DAC happy while preserving the
        // resonant boost up to ~0 dBFS.
        constexpr float kOutDrive = 0.85f;
        constexpr float kOutInv   = 1.0f / 0.85f;

        // --- Process each sample ---
        for (int i = 0; i < numSamples; ++i) {

            // === LEFT CHANNEL ===
            const float inL = left[i];
            const float bpL = bpL_.process(inL, coeffs);

            // Resonant-peak boost → op-amp growl → equal-power blend → output limiter
            const float peakL  = bpL * bpGain;
            const float growlL = tanhf(peakL * kGrowlDrive) * kGrowlInv;
            const float mixedL = wet * growlL + dry * inL;
            left[i] = tanhf(mixedL * kOutDrive) * kOutInv;

            // === RIGHT CHANNEL — identical biquad with independent state ===
            const float inR = right[i];
            const float bpR = bpR_.process(inR, coeffs);

            const float peakR  = bpR * bpGain;
            const float growlR = tanhf(peakR * kGrowlDrive) * kGrowlInv;
            const float mixedR = wet * growlR + dry * inR;
            right[i] = tanhf(mixedR * kOutDrive) * kOutInv;
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return { 0.0f, 1.0f, 1.0f };     // Mix: 0=dry, 1=wet, default=full wet
            case 1: return { 0.0f, 1.0f, kCrybabyQKnobDefault }; // Q knob (→ Q 1–10, default Q≈7)
            case 2: return { 0.0f, 1.0f, 0.5f };     // Wah position: 0=heel, 1=toe, default=mid
            default: return { 0.0f, 1.0f, 0.5f };
        }
    }

    const char* getParameterName(int idx) override
    {
        switch (idx) {
            case 0: return "Mix";
            case 1: return "Q";
            case 2: return "Wah";
            default: return nullptr;
        }
    }

    void setParamValue(int idx, float value) override
    {
        // Called from audio thread — no synchronization needed per SDK contract.
        switch (idx) {
            case 0: mix_    = value; break;  // Left  — Mix
            case 1: q_knob_ = value; break;  // Mid   — Q resonance
            case 2: wahPos_ = value; break;  // Right — Wah position (expression pedal heel=0 toe=1)
            default: break;
        }
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress)) {
            // Short press: toggle bypass on/off
            bypassed_ = !bypassed_;
            if (!bypassed_) {
                // Clear filter state on re-engage to prevent a pop from stale values.
                clearFilterState();
            }
        } else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold)) {
            // Hold: toggle between Crybaby and Vox modes
            // (Note: the second physical footswitch is firmware-reserved and not accessible
            //  to patch code in the current SDK. Hold is used here as the mode switch.)
            mode_ = (mode_ == WahMode::kCrybaby) ? WahMode::kVox : WahMode::kCrybaby;
            // Clear filter state to avoid a transient from the fc jump between modes.
            clearFilterState();
        }
    }

    Color getStateLedColor() override
    {
        // Four distinct LED states: active/bypassed × Crybaby/Vox
        // Dim version of the mode color when bypassed so the player knows which mode
        // they will return to on re-engage — regardless of bypass state.
        if (mode_ == WahMode::kCrybaby)
            return bypassed_ ? Color::kDarkRed    : Color::kRed;
        else
            return bypassed_ ? Color::kDimYellow  : Color::kLightYellow;
    }

private:
    // Knob / parameter state (0.0–1.0 normalized, as delivered by setParamValue)
    float mix_    = 1.0f;                  // param 0 — dry/wet blend
    float q_knob_ = kCrybabyQKnobDefault;  // param 1 — Q knob position → Q = 1 + q_knob_*9
    float wahPos_ = 0.5f;                  // param 2 — sweep position (expression pedal)

    // Mode and bypass
    WahMode mode_    = WahMode::kCrybaby;
    bool    bypassed_= false;

    // Biquad filter state — one instance per channel. Same state count (2
    // floats each) as the Chamberlin low_/band_ pair this replaced.
    dsp::BandpassBiquad bpL_;
    dsp::BandpassBiquad bpR_;

    // Clear filter state to prevent pops on bypass-off or mode change.
    void clearFilterState()
    {
        bpL_.reset();
        bpR_.reset();
    }
};

// C ABI singleton required by PatchCppWrapper.cpp
Patch* Patch::getInstance()
{
    static Wah instance;
    return &instance;
}
