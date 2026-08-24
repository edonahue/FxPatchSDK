// Funk Machine Envelope Filter — Polyend Endless SDK patch
//
// Touch-sensitive funk filter for bass, clav, clean guitar, and keyboards.
// The input envelope opens (Down) or closes (Up) a resonant state-variable
// filter while the Right knob / expression pedal shifts the entire sweep
// window up or down.
//
// Controls:
//   Left  knob  — Sensitivity: how strongly playing dynamics move the filter
//   Mid   knob  — Resonance: broad/fat to sharp/vocal
//   Right knob  — Bias: shifts the whole envelope sweep window; expression mapped
//   Footswitch press — bypass
//   Footswitch hold  — advances a 4-state Bass/GuitarKeys x Down/Up cycle:
//     Bass-Down (default) -> GuitarKeys-Down -> GuitarKeys-Up -> Bass-Up -> loop.
//     Voice flips on odd-numbered holds, direction flips on even-numbered
//     holds, so the very first hold from power-on reproduces the original
//     Bass<->GuitarKeys toggle exactly. See "Decision 4b" in the walkthrough
//     doc for why this ordering (not a plain 2-bit binary count) was chosen,
//     and for the honest tradeoff: rapid repeated Bass<->GuitarKeys ping-pong
//     no longer round-trips in 2 holds once Up has been visited.
//
// See docs/funk-machine-envelope-filter-build-walkthrough.md.

#include "../source/Patch.h"
#include "../source/dsp/filter_coeff.h"
#include "../source/dsp/soft_limit.h"

#include <cmath>

namespace {
constexpr float kFs = static_cast<float>(Patch::kSampleRate);

constexpr float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// Detector timing. Attack is intentionally quick enough to catch bass/clav
// transients; release is slow enough to produce a musical vowel rather than
// chatter between individual waveform cycles.
constexpr float kAttackMs  = 4.0f;
constexpr float kReleaseMs = 95.0f;

// Filter frequency is derived from the audio-rate detector only every eight
// samples. That is a 6 kHz control rate: far faster than the envelope can move,
// while avoiding powf/sinf in the inner loop on every sample.
constexpr int kControlInterval = 8;

float onePoleTimeCoeff(float ms)
{
    return expf(-1.0f / (0.001f * ms * kFs));
}
}

enum class FunkVoice
{
    kBass,
    kGuitarKeys,
};

enum class FunkDirection
{
    kDown,  // envelope opens the filter (louder -> brighter) -- the original behavior
    kUp,    // envelope closes the filter (louder -> darker) -- Mu-Tron-III-style inversion
};

namespace {
// Gray-code Hold cycle: index advances +1 (mod 4) per Hold press. Voice
// flips on odd-numbered presses, direction flips on even-numbered presses,
// so every single press changes exactly one axis, never both at once, and
// the very first Hold from the power-on default (state 0) reproduces the
// original Bass->GuitarKeys toggle exactly -- a player who never touches
// Up mode sees no behavior change on the one gesture everyone already uses.
constexpr FunkVoice kVoiceForHoldState[4] = {
    FunkVoice::kBass, FunkVoice::kGuitarKeys, FunkVoice::kGuitarKeys, FunkVoice::kBass,
};
constexpr FunkDirection kDirectionForHoldState[4] = {
    FunkDirection::kDown, FunkDirection::kDown, FunkDirection::kUp, FunkDirection::kUp,
};
}

class FunkMachine final : public Patch
{
public:
    void init() override
    {
        sensitivity_ = 0.58f;
        resonance_   = 0.56f;
        bias_        = 0.40f;
        holdState_   = 0;
        voice_       = kVoiceForHoldState[holdState_];
        direction_   = kDirectionForHoldState[holdState_];
        bypassed_    = false;

        attackCoeff_  = onePoleTimeCoeff(kAttackMs);
        releaseCoeff_ = onePoleTimeCoeff(kReleaseMs);
        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> /*buf*/) override
    {
        // Scalar state only; no external working buffer required.
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_)
            return;

        const float sens = clamp01(sensitivity_);
        const float res  = clamp01(resonance_);
        const float bias = clamp01(bias_);

        // Sensitivity is intentionally nonlinear: the first half remains useful
        // for hot bass/keyboard signals, while the upper half can still open the
        // filter from lower-output guitar playing.
        const float envelopeGain = 1.4f + 8.6f * sens * sens;

        // Resonance maps to Q 1.2..7.5. Staying below the wah's extreme Q keeps
        // the envelope sweep punchy and prevents note-on spikes from becoming
        // brittle or unstable.
        const float q  = 1.2f + 6.3f * res;
        const float q1 = 1.0f / q;
        const float bpGain = 2.0f * q1 * 1.75f;

        // Bias moves the window but deliberately keeps the top frequency under
        // ~3 kHz, where this repo's Chamberlin SVF precedent remains comfortable
        // at the Q values used here.
        float fcMin;
        float fcMax;
        float dryFoundation;
        if (voice_ == FunkVoice::kBass) {
            fcMin = 70.0f * powf(4.0f, bias);      // 70 .. 280 Hz
            fcMax = 900.0f * powf(2.4f, bias);    // 900 .. 2160 Hz
            dryFoundation = 0.28f;
        } else {
            fcMin = 150.0f * powf(3.0f, bias);    // 150 .. 450 Hz
            fcMax = 1600.0f * powf(1.8f, bias);   // 1600 .. 2880 Hz
            dryFoundation = 0.10f;
        }
        const float fcRatio = fcMax / fcMin;

        // Down (original): fc = fcMin * fcRatio^envNorm -- quiet/idle -> fcMin
        // (dark), loud -> fcMax (bright), envelope opens the filter.
        // Up (Mu-Tron-III-style inversion): the exponent flips to
        // (1 - envNorm), which is algebraically fc = fcMax * fcRatio^-envNorm
        // -- quiet/idle -> fcMax (bright), loud -> fcMin (dark), envelope
        // closes the filter. Same fcMin/fcMax/dryFoundation per voice either
        // way; only which end of the window the envelope drives toward changes.
        const bool isUp = (direction_ == FunkDirection::kUp);

        for (size_t i = 0; i < left.size(); ++i) {
            const float inL = left[i];
            const float inR = right[i];

            // Linked stereo detector: preserve stereo image by driving both
            // filters from one control envelope derived from the hotter channel.
            const float level = fmaxf(fabsf(inL), fabsf(inR));
            const float coeff = (level > envelope_) ? attackCoeff_ : releaseCoeff_;
            envelope_ = coeff * envelope_ + (1.0f - coeff) * level;

            // Convert detector amplitude to a bounded 0..1 control signal.
            // The rational saturator avoids an expensive per-sample tanh/pow and
            // naturally compresses very hot line-level keyboard transients.
            const float driven = envelope_ * envelopeGain;
            const float envNorm = driven / (1.0f + driven);

            // Recompute the SVF frequency coefficient at a 6 kHz control rate.
            // Filter state itself still updates every sample, so there is no
            // decimation of the audio path.
            if (controlCountdown_ <= 0) {
                const float exponent = isUp ? (1.0f - envNorm) : envNorm;
                const float fc = fcMin * powf(fcRatio, exponent);
                f1_ = dsp::svfF1(fc, kFs);
                controlCountdown_ = kControlInterval;
            }
            --controlCountdown_;

            // LEFT state-variable filter.
            lowL_ += f1_ * bandL_;
            const float hiL = inL - lowL_ - q1 * bandL_;
            bandL_ += f1_ * hiL;

            // RIGHT state-variable filter.
            lowR_ += f1_ * bandR_;
            const float hiR = inR - lowR_ - q1 * bandR_;
            bandR_ += f1_ * hiR;

            // Normalize raw Chamberlin bandpass peak (roughly Q/2) then add a
            // moderate vocal boost. Bass voice retains a fixed clean foundation
            // so fundamentals survive even at high resonance.
            const float wetL = bandL_ * bpGain;
            const float wetR = bandR_ * bpGain;

            const float outL = wetL * (1.0f - dryFoundation) + inL * dryFoundation;
            const float outR = wetR * (1.0f - dryFoundation) + inR * dryFoundation;

            left[i]  = dsp::softLimit(outL, 0.90f, 0.24f, 0.10f);
            right[i] = dsp::softLimit(outR, 0.90f, 0.24f, 0.10f);
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.58f};
            case 1: return {0.0f, 1.0f, 0.56f};
            case 2: return {0.0f, 1.0f, 0.40f};
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    const char* getParameterName(int idx) override
    {
        switch (idx) {
            case 0: return "Sens";
            case 1: return "Reso";
            case 2: return "Bias";
            default: return nullptr;
        }
    }

    void setParamValue(int idx, float value) override
    {
        value = clamp01(value);
        switch (idx) {
            case 0: sensitivity_ = value; break;
            case 1: resonance_ = value; break;
            case 2: bias_ = value; break;
            default: break;
        }
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress)) {
            bypassed_ = !bypassed_;
            if (!bypassed_)
                clearState();
        } else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold)) {
            // Advances the Gray-code cycle by one step -- voice flips on odd
            // presses, direction flips on even presses, never both at once.
            // Whichever axis changes, only the mapping window or its
            // exponent changes, not the SVF topology itself, so the clearing
            // policy is identical to the original voice-only toggle: clear
            // filter state (the largest fc jump this patch can produce,
            // especially at extreme envNorm, happens right here), leave the
            // detector (envelope_) running so the gesture stays natural.
            holdState_ = (holdState_ + 1) % 4;
            voice_     = kVoiceForHoldState[holdState_];
            direction_ = kDirectionForHoldState[holdState_];
            clearFilterState();
            controlCountdown_ = 0;
        }
    }

    Color getStateLedColor() override
    {
        constexpr Color kActiveForHoldState[4] = {
            Color::kLightGreen, Color::kLightBlueColor, Color::kDarkCobalt, Color::kPastelGreen,
        };
        constexpr Color kBypassedForHoldState[4] = {
            Color::kDarkLime, Color::kDimCyan, Color::kDimCyan, Color::kDimGreen,
        };
        return bypassed_ ? kBypassedForHoldState[holdState_] : kActiveForHoldState[holdState_];
    }

private:
    float sensitivity_ = 0.58f;
    float resonance_   = 0.56f;
    float bias_        = 0.40f;

    float attackCoeff_  = 0.0f;
    float releaseCoeff_ = 0.0f;
    float envelope_     = 0.0f;
    float f1_           = 0.0f;
    int controlCountdown_ = 0;

    float lowL_  = 0.0f;
    float bandL_ = 0.0f;
    float lowR_  = 0.0f;
    float bandR_ = 0.0f;

    int holdState_ = 0;  // index into kVoiceForHoldState/kDirectionForHoldState
    FunkVoice voice_ = FunkVoice::kBass;
    FunkDirection direction_ = FunkDirection::kDown;
    bool bypassed_ = false;

    void clearFilterState()
    {
        lowL_ = 0.0f;
        bandL_ = 0.0f;
        lowR_ = 0.0f;
        bandR_ = 0.0f;
    }

    void clearState()
    {
        envelope_ = 0.0f;
        f1_ = 0.0f;
        controlCountdown_ = 0;
        clearFilterState();
    }
};

Patch* Patch::getInstance()
{
    static FunkMachine instance;
    return &instance;
}
