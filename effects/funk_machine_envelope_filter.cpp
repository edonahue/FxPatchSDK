// Funk Machine Envelope Filter — Polyend Endless SDK patch
//
// Touch-sensitive funk filter for bass, clav, clean guitar, and keyboards.
// The input envelope opens a resonant state-variable filter while the Right
// knob / expression pedal shifts the entire sweep window up or down.
//
// Controls:
//   Left  knob  — Sensitivity: how strongly playing dynamics open the filter
//   Mid   knob  — Resonance: broad/fat to sharp/vocal
//   Right knob  — Bias: shifts the whole envelope sweep window; expression mapped
//   Footswitch press — bypass
//   Footswitch hold  — Bass <-> Guitar/Keys voicing
//
// See docs/funk-machine-envelope-filter-build-walkthrough.md.

#include "../source/Patch.h"
#include "../source/dsp/soft_limit.h"

#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
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

class FunkMachine final : public Patch
{
public:
    void init() override
    {
        sensitivity_ = 0.58f;
        resonance_   = 0.56f;
        bias_        = 0.40f;
        voice_       = FunkVoice::kBass;
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

            float fcMin;
            float fcMax;
            float dryFoundation;
            if (voice_ == FunkVoice::kBass) {
                fcMin = 90.0f;
                fcMax = 1450.0f;
                dryFoundation = 0.28f;
            } else {
                fcMin = 180.0f;
                fcMax = 2600.0f;
                dryFoundation = 0.10f;
            }

            // Bias shifts both ends of the window upward without replacing the
            // touch envelope. Heel gives the deepest quack; toe produces a
            // brighter clav/lead voice. Mapping is exponential in frequency.
            const float biasOctaves = 1.45f * bias;
            const float biasScale = exp2f(biasOctaves);
            fcMin *= biasScale;
            fcMax *= biasScale;

            const float ratio = fcMax / fcMin;
            const float fc = fcMin * powf(ratio, envNorm);
            const float f1 = 2.0f * sinf(kPi * fc / kFs);

            // LEFT state-variable filter.
            lowL_ += f1 * bandL_;
            const float hiL = inL - lowL_ - q1 * bandL_;
            bandL_ += f1 * hiL;

            // RIGHT state-variable filter.
            lowR_ += f1 * bandR_;
            const float hiR = inR - lowR_ - q1 * bandR_;
            bandR_ += f1 * hiR;

            // Normalize raw Chamberlin bandpass peak (roughly Q/2) then add a
            // moderate vocal boost. Bass voice retains a fixed clean foundation
            // so fundamentals survive even at high resonance.
            const float bpGain = 2.0f * q1 * 1.75f;
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
            voice_ = (voice_ == FunkVoice::kBass) ? FunkVoice::kGuitarKeys : FunkVoice::kBass;
            clearFilterState();
        }
    }

    Color getStateLedColor() override
    {
        if (voice_ == FunkVoice::kBass)
            return bypassed_ ? Color::kDimGreen : Color::kLightGreen;
        return bypassed_ ? Color::kDimCyan : Color::kLightBlueColor;
    }

private:
    float sensitivity_ = 0.58f;
    float resonance_   = 0.56f;
    float bias_        = 0.40f;

    float attackCoeff_  = 0.0f;
    float releaseCoeff_ = 0.0f;
    float envelope_     = 0.0f;

    float lowL_  = 0.0f;
    float bandL_ = 0.0f;
    float lowR_  = 0.0f;
    float bandR_ = 0.0f;

    FunkVoice voice_ = FunkVoice::kBass;
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
        clearFilterState();
    }
};

Patch* Patch::getInstance()
{
    static FunkMachine instance;
    return &instance;
}
