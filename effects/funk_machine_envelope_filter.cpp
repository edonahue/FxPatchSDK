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
#include "../source/dsp/biquad.h"
#include "../source/dsp/clamp.h"
#include "../source/dsp/soft_limit.h"

#include <cmath>
#include <cstdint>

namespace {
using dsp::clamp01;
constexpr float kFs = static_cast<float>(Patch::kSampleRate);

// Detector timing. Attack is intentionally quick enough to catch bass/clav
// transients; release is slow enough to produce a musical vowel rather than
// chatter between individual waveform cycles.
constexpr float kAttackMs  = 4.0f;
constexpr float kReleaseMs = 95.0f;

// Filter frequency is derived from the audio-rate detector only every eight
// samples. That is a 6 kHz control rate: far faster than the envelope can move,
// while avoiding powf/sinf in the inner loop on every sample.
constexpr int kControlInterval = 8;
constexpr float kPi = 3.14159265359f;

float onePoleTimeCoeff(float ms)
{
    return expf(-1.0f / (0.001f * ms * kFs));
}

// --- libm-free control-rate maths -------------------------------------------
//
// The frequency chain runs at a 6 kHz control rate (every eight samples): a
// 12,000-times/second budget for anything that touches libm. No Polyend or
// Playground patch in docs/endl-corpus-study.md's corpus links a transcendental
// at all, which prompted replacing these.
//
// Accuracy was measured before adopting, not assumed:
// tests/funk_machine_approx_probe.cpp sweeps both voices across the full
// bias x envelope space (80,802 points) and reports a worst-case cutoff error
// of 0.15 cents for the exp2 substitution. Pitch discrimination tops out near
// 1 cent and filter cutoff is far less sensitive than pitch.
//
// Kept file-local rather than promoted to source/dsp/: this is the only effect
// that needs them so far, and the repo's bar is two users.

// 2^x without libm. The integer part is written straight into the IEEE-754
// exponent field; the fraction uses the truncated series for e^(f ln2).
inline float fastExp2(float x)
{
    const float xi = __builtin_floorf(x);          // vrintm.f32
    const float xf = x - xi;

    float p = 0.0013333f;                          // (ln2)^5/120
    p = p * xf + 0.0096181f;                       // (ln2)^4/24
    p = p * xf + 0.0555041f;                       // (ln2)^3/6
    p = p * xf + 0.2402265f;                       // (ln2)^2/2
    p = p * xf + 0.6931472f;                       // ln2
    p = p * xf + 1.0f;

    // The reachable exponent range here is 0..4, but clamp regardless so a
    // stray value cannot synthesise a denormal or an infinity.
    int e = static_cast<int>(xi);
    if (e < -60) { e = -60; }
    if (e > 60)  { e = 60; }
    const uint32_t bits = static_cast<uint32_t>(e + 127) << 23;
    return p * __builtin_bit_cast(float, bits);
}

// sin(x)/cos(x) for small x. fc never exceeds ~2.9 kHz, so pi*fc/fs (this
// patch's own half-angle -- see below) stays below 0.19 rad and the first
// omitted term in each series is under 2.1e-6.
inline float sinSmall(float x)
{
    return x - (x * x * x) * (1.0f / 6.0f);
}

inline float cosSmall(float x)
{
    const float x2 = x * x;
    return 1.0f - x2 * 0.5f + x2 * x2 * (1.0f / 24.0f);
}

// Libm-free RBJ constant-peak-gain bandpass coefficients (see
// dsp::rbjBandpassCoeffs, source/dsp/filter_coeff.h, which this mirrors
// exactly except for how sin(w0)/cos(w0) are obtained).
//
// RBJ's w0 = 2*pi*fc/fs is double the angle sinSmall/cosSmall above were
// validated for (pi*fc/fs, this patch's own control-rate half-angle) -- but
// w0/2 is exactly that angle, so sin(w0)/cos(w0) are derived via the double-
// angle identities from sinSmall(w0/2)/cosSmall(w0/2) rather than fitting a
// new series over a wider range. Verified end-to-end (not just the
// intermediate sin/cos values) in tests/funk_machine_biquad_accuracy_probe.cpp,
// which evaluates the exact z-transform magnitude response |H(f)| of this
// function's output rather than a time-domain peak search (the latter's
// settling/grid artifacts produced a spurious 13.8-cent reading during
// development that the exact method showed was actually 0.02 cents -- see
// that file's own comments). Worst genuine error across this patch's full
// fc range (70-2900 Hz) and Q range (1.2-7.5) is 1.387 cents, consistent
// with float32 rounding in the coefficients themselves rather than the
// small-angle approximation.
inline dsp::BiquadCoeffs libmFreeBandpassCoeffs(float fc, float q)
{
    const float half  = kPi * fc / kFs;  // == w0/2
    const float sh    = sinSmall(half);
    const float ch    = cosSmall(half);
    const float sinW0 = 2.0f * sh * ch;
    const float cosW0 = 1.0f - 2.0f * sh * sh;
    const float alpha = sinW0 / (2.0f * q);
    const float invA0 = 1.0f / (1.0f + alpha);

    return {
        alpha * invA0,
        -alpha * invA0,
        -2.0f * cosW0 * invA0,
        (1.0f - alpha) * invA0,
    };
}

// log2 of each literal in the frequency windows below. These are constants in
// the source, so their logarithms are constants too -- which is what removes
// the runtime logarithm the pow-to-exp2 rewrite would otherwise need.
constexpr float kLog2_70   = 6.1292830f;
constexpr float kLog2_900  = 9.8137811f;
constexpr float kLog2_2p4  = 1.2630344f;
constexpr float kLog2_150  = 7.2288187f;
constexpr float kLog2_1600 = 10.6438562f;
constexpr float kLog2_3    = 1.5849625f;
constexpr float kLog2_1p8  = 0.8479969f;
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

        // The RBJ biquad's peak is exactly 1.0x input at fc for any Q, by
        // construction, so kResonanceGain is the whole story -- no per-Q
        // correction needed. (The Chamberlin SVF this replaced had the same
        // bug found in wah.cpp: its "2*q1*1.75" formula assumed a raw peak of
        // Q/2, but direct measurement shows the true raw peak is Q, so the
        // formula's Q-dependence happened to cancel by coincidence, giving an
        // actual old peak of 2*1.75=3.5, not the Q-dependent value its shape
        // suggested. kResonanceGain is set to that same 3.5 directly, so this
        // reproduces the real old loudness rather than the old formula's
        // never-quite-true intent. See wah-build-walkthrough.md's 2026-08-24
        // addendum for the full derivation, done first on that effect.)
        constexpr float kResonanceGain = 3.5f;
        const float bpGain = kResonanceGain;

        // Bias moves the window but deliberately keeps the top frequency under
        // ~3 kHz, where this repo's Chamberlin SVF precedent remains comfortable
        // at the Q values used here.
        // The window is carried as log2(Hz). 4, 2.4, 3 and 1.8 are literals, so
        // powf(base, bias) is exp2(bias * log2(base)) with a constant
        // multiplier -- and in the log2 domain the whole map is affine in bias,
        // so no logarithm is needed at runtime either.
        float log2FcMin;
        float log2FcMax;
        float dryFoundation;
        if (voice_ == FunkVoice::kBass) {
            log2FcMin = kLog2_70 + 2.0f * bias;              // 70 .. 280 Hz
            log2FcMax = kLog2_900 + kLog2_2p4 * bias;        // 900 .. 2160 Hz
            dryFoundation = 0.28f;
        } else {
            log2FcMin = kLog2_150 + kLog2_3 * bias;          // 150 .. 450 Hz
            log2FcMax = kLog2_1600 + kLog2_1p8 * bias;       // 1600 .. 2880 Hz
            dryFoundation = 0.10f;
        }
        const float log2FcSpan = log2FcMax - log2FcMin;      // == log2(fcRatio)

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
            // Builtins, not fmaxf/fabsf: this runs every sample, and under
            // -fno-builtin the plain names are three calls into libm here.
            // The builtins are vabs.f32 x2 + vmaxnm.f32, and exact.
            const float level = __builtin_fmaxf(__builtin_fabsf(inL),
                                                __builtin_fabsf(inR));
            const float coeff = (level > envelope_) ? attackCoeff_ : releaseCoeff_;
            envelope_ = coeff * envelope_ + (1.0f - coeff) * level;

            // Convert detector amplitude to a bounded 0..1 control signal.
            // The rational saturator avoids an expensive per-sample tanh/pow and
            // naturally compresses very hot line-level keyboard transients.
            const float driven = envelope_ * envelopeGain;
            const float envNorm = driven / (1.0f + driven);

            // Recompute the biquad coefficients at a 6 kHz control rate.
            // Filter state itself still updates every sample, so there is no
            // decimation of the audio path.
            if (controlCountdown_ <= 0) {
                const float exponent = isUp ? (1.0f - envNorm) : envNorm;
                // Same geometric sweep as before, evaluated in the log2 domain:
                // fc = fcMin * fcRatio^exponent.
                const float fc = fastExp2(log2FcMin + exponent * log2FcSpan);
                coeffs_ = libmFreeBandpassCoeffs(fc, q);
                controlCountdown_ = kControlInterval;
            }
            --controlCountdown_;

            const float bpL = bpL_.process(inL, coeffs_);
            const float bpR = bpR_.process(inR, coeffs_);

            // Resonant-peak boost. Bass voice retains a fixed clean
            // foundation so fundamentals survive even at high resonance.
            const float wetL = bpL * bpGain;
            const float wetR = bpR * bpGain;

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
    dsp::BiquadCoeffs coeffs_{};
    int controlCountdown_ = 0;

    // Biquad filter state -- one instance per channel. Same state count (2
    // floats each) as the Chamberlin lowL_/bandL_/lowR_/bandR_ this replaced.
    dsp::BandpassBiquad bpL_;
    dsp::BandpassBiquad bpR_;

    int holdState_ = 0;  // index into kVoiceForHoldState/kDirectionForHoldState
    FunkVoice voice_ = FunkVoice::kBass;
    FunkDirection direction_ = FunkDirection::kDown;
    bool bypassed_ = false;

    void clearFilterState()
    {
        bpL_.reset();
        bpR_.reset();
    }

    void clearState()
    {
        envelope_ = 0.0f;
        coeffs_ = dsp::BiquadCoeffs{};
        controlCountdown_ = 0;
        clearFilterState();
    }
};

Patch* Patch::getInstance()
{
    static FunkMachine instance;
    return &instance;
}
