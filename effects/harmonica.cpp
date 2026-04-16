// Harmonica — Polyend Endless SDK patch
//
// Blues bullet-mic harmonica voicing of an electric guitar. Transforms the
// guitar's timbre toward a cupped Shure Green Bullet mic feeding a cranked
// tube amp — Chicago blues territory (Little Walter, Sonny Boy Williamson II).
//
// The SDK exposes no pitch shifter, so this patch does not transpose pitch.
// The illusion depends on playing style: neck pickup, tone rolled to ~3,
// single-note lines above the 5th fret, fingerstyle, no open low strings.
//
// Controls:
//   Left  knob  — Tone / Cup        (loose & bright ↔ tight & dark)
//   Mid   knob  — Reed / Drive      (light reed ↔ cranked bullet-amp breakup)
//   Right knob  — Waa / Cup sweep   (hand-muting formant sweep; expression pedal
//                                    overrides this knob at the firmware layer)
//   Footswitch press — Bypass toggle
//   Footswitch hold  — Toggle Open ↔ Cupped voicing
//   LED:
//     LightYellow = Cupped, engaged   (brass/reed warmth)
//     DimYellow   = Cupped, bypassed
//     LightBlue   = Open, engaged     (airy/open hands)
//     DimBlue     = Open, bypassed
//
// Signal chain (per sample, stereo):
//   pre-HPF → low-shelf split → formant-1 BP (swept) → formant-2 BP (fixed)
//     → asymmetric reed saturation (body/edge split)
//     → post-LPF (Green Bullet rolloff)
//     → tremolo AM (dual-reed beat)
//     → micro-chorus (reed detune)
//     → DC blocker → soft limit
//
// Reuses patterns from:
//   effects/wah.cpp          — Chamberlin SVF, log fc sweep, clearState on re-engage
//   effects/tube_screamer.cpp — body/edge LP split before tanh, softLimit, HP coefficient
//   effects/chorus.cpp       — fractional-delay line layout in the working buffer

#include "../source/Patch.h"

#include <cmath>
#include <cstddef>

namespace {
constexpr float kPi     = 3.14159265f;
constexpr float kTwoPi  = 6.28318531f;
constexpr float kFs     = static_cast<float>(Patch::kSampleRate);

// Formant-1 (hand-cup) frequency sweep range — log-taper.
constexpr float kFormant1FcMin    = 320.0f;   // heel — cupped/closed
constexpr float kFormant1FcMaxOp  = 2500.0f;  // toe  — Open voicing
constexpr float kFormant1FcMaxCup = 2000.0f;  // toe  — Cupped voicing

// Formant-2 (nasal/reed-plate cavity) — fixed peak.
constexpr float kFormant2Fc = 1700.0f;
constexpr float kFormant2Q  = 2.0f;

// Low-shelf crossover: drops sub-harmonica lows remaining after the pre-HPF.
constexpr float kLowShelfFc = 220.0f;

// Reed-saturation body/edge split (pattern from tube_screamer.cpp).
constexpr float kReedSplitFc = 700.0f;

// Tremolo (dual-reed beat). Fixed rate; depth varies by voicing.
constexpr float kTremHz         = 5.8f;
constexpr float kTremPhaseRightOffset = 25.0f / 360.0f; // 25° stereo spread

// Micro-chorus (reed detune). Kept deliberately subtle — not a chorus effect.
constexpr float kChorusRateHz    = 0.45f;
constexpr float kChorusCenterMs  = 7.0f;
constexpr float kChorusDepthMs   = 0.35f;
constexpr int   kChorusLen       = 480;  // 10 ms @ 48 kHz — ample for center + depth

// DC blocker (scrubs DC from asymmetric clipper).
constexpr float kDcBlockFc = 25.0f;

// Voicing constants. The TONE knob applies a small trim on top of these; the
// hold-toggle switches between the two voicing presets.
struct Voicing {
    float preHpFc;        // Hz — pre-HPF cutoff
    float lowShelfCutDb;  // dB — negative = cut
    float form1Q;         // Chamberlin Q
    float form1Gain;      // peak amplitude after unity-peak normalization
    float form2Mix;       // additive mix of the fixed formant
    float postLpFc;       // Hz — Green Bullet rolloff
    float clipGain;       // headroom scalar into tanh
    float asymmetry;      // >1 = negative swing clips harder (reed valve asymmetry)
    float tremDepthDb;    // dB — peak AM depth
    float chorusWet;      // 0..1 micro-chorus mix
    float form1FcMax;     // Hz — WAA toe position
};

constexpr Voicing kOpen = {
    120.0f,  -2.0f,  3.0f,  3.0f,  0.35f,  2800.0f,  2.2f,  1.10f,  0.8f,  0.08f,  kFormant1FcMaxOp
};
constexpr Voicing kCupped = {
    160.0f,  -5.0f,  6.0f,  4.2f,  0.50f,  1900.0f,  3.4f,  1.30f,  1.4f,  0.16f,  kFormant1FcMaxCup
};

inline float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// 1-pole highpass coefficient (matches effects/tube_screamer.cpp:62).
inline float hpCoeff(float fc)
{
    return 1.0f / (1.0f + kTwoPi * fc / kFs);
}

// 1-pole lowpass coefficient (matches effects/tube_screamer.cpp:66).
inline float lpCoeff(float fc)
{
    const float omega = kTwoPi * fc / kFs;
    return omega / (1.0f + omega);
}

inline float dbToAmp(float db)
{
    return powf(10.0f, db / 20.0f);
}

// Safety soft limiter (pattern from effects/tube_screamer.cpp:49).
inline float softLimit(float v)
{
    const float a = fabsf(v);
    if (a <= 0.90f) return v;
    const float sign = v < 0.0f ? -1.0f : 1.0f;
    return sign * (0.90f + 0.10f * tanhf((a - 0.90f) / 0.25f));
}
} // namespace

enum class HarmMode { kCupped, kOpen };

class HarmonicaPatch final : public Patch
{
public:
    void init() override
    {
        tone_    = 0.55f;
        reed_    = 0.48f;
        waa_     = 0.45f;
        mode_    = HarmMode::kCupped;
        bypassed_ = false;

        // Note: delayL_/delayR_ are set by setWorkingBuffer and are intentionally
        // not touched here — init() is called after setWorkingBuffer by the host,
        // and clobbering the pointers would leave processAudio in a dead state.
        clearState();
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buf) override
    {
        float* ptr = buf.data();
        delayL_ = ptr;
        delayR_ = ptr + kChorusLen;
        for (int i = 0; i < kChorusLen; ++i) {
            delayL_[i] = 0.0f;
            delayR_[i] = 0.0f;
        }
    }

    void processAudio(std::span<float> left, std::span<float> right) override
    {
        if (bypassed_ || !delayL_) return;

        const float tone = clamp01(tone_);
        const float reed = clamp01(reed_);
        const float waa  = clamp01(waa_);

        const Voicing& base = (mode_ == HarmMode::kCupped) ? kCupped : kOpen;

        // --- TONE knob (Left): small trim on top of the base voicing ---
        // 0 = airier, brighter, less cut; 1 = tighter, darker, more cut.
        const float toneDepth      = 0.4f + 0.6f * tone;                // 0.4..1.0
        const float lowShelfCutDb  = base.lowShelfCutDb * toneDepth;
        const float lowShelfGain   = dbToAmp(lowShelfCutDb);             // <1 (cut)
        const float form2Mix       = base.form2Mix + 0.10f * (tone - 0.5f);
        const float postLpFcFinal  = base.postLpFc - 500.0f * (tone - 0.5f);

        // --- REED knob (Mid): saturation amount, with asymmetric tanh ---
        const float reedCurve = 0.08f + 0.92f * powf(reed, 1.05f);       // 0.08..1.0
        const float baseDrive = 1.4f + 2.1f * reedCurve;                 // 1.4..3.5
        const float clipGain  = base.clipGain * (0.55f + 0.90f * reedCurve);
        const float edgeGain  = baseDrive * 1.4f;

        // --- WAA knob (Right / expression): log-sweep formant-1 center ---
        const float fcRatio = base.form1FcMax / kFormant1FcMin;
        const float f1Fc    = kFormant1FcMin * powf(fcRatio, waa);
        const float f1      = 2.0f * sinf(kPi * f1Fc / kFs);
        const float q1      = 1.0f / base.form1Q;
        // Peak of Chamberlin bandpass is Q/2. Normalize to 1.0 via (2/Q) then
        // scale by voicing peak gain (same logic as effects/wah.cpp:125).
        const float bp1Gain = base.form1Gain * 2.0f * q1;

        // --- Formant-2 (fixed) ---
        const float f2      = 2.0f * sinf(kPi * kFormant2Fc / kFs);
        const float q2      = 1.0f / kFormant2Q;
        const float bp2Gain = 2.0f * q2;  // normalized to unity peak

        const float preHpAlpha   = hpCoeff(base.preHpFc);
        const float shelfLpAlpha = lpCoeff(kLowShelfFc);
        const float splitLpAlpha = lpCoeff(kReedSplitFc);
        const float postLpAlpha  = lpCoeff(postLpFcFinal);
        const float dcAlpha      = hpCoeff(kDcBlockFc);

        // Tremolo depth as linear AM around 1.0.
        const float tremDepth     = dbToAmp(base.tremDepthDb) - 1.0f;   // e.g. 0.096 at 0.8 dB
        const float tremPhaseInc  = kTremHz / kFs;

        // Chorus (micro-detune).
        const float chorusPhaseInc = kChorusRateHz / kFs;
        const float chorusCenter   = kChorusCenterMs * 0.001f * kFs;    // samples
        const float chorusDepth    = kChorusDepthMs  * 0.001f * kFs;
        const float chorusWet      = base.chorusWet;
        const float chorusDry      = 1.0f - chorusWet;

        const size_t n = left.size();
        for (size_t i = 0; i < n; ++i)
        {
            left[i]  = processChannel(0, left[i],
                                      preHpAlpha, shelfLpAlpha, lowShelfGain,
                                      f1, q1, bp1Gain, f2, q2, bp2Gain, form2Mix,
                                      splitLpAlpha, baseDrive, edgeGain, clipGain, base.asymmetry,
                                      postLpAlpha,
                                      tremDepth, tremPhase_,
                                      chorusDepth, chorusCenter, chorusPhaseL_,
                                      chorusDry, chorusWet,
                                      dcAlpha, delayL_);

            right[i] = processChannel(1, right[i],
                                      preHpAlpha, shelfLpAlpha, lowShelfGain,
                                      f1, q1, bp1Gain, f2, q2, bp2Gain, form2Mix,
                                      splitLpAlpha, baseDrive, edgeGain, clipGain, base.asymmetry,
                                      postLpAlpha,
                                      tremDepth, tremPhase_ + kTremPhaseRightOffset,
                                      chorusDepth, chorusCenter, chorusPhaseR_,
                                      chorusDry, chorusWet,
                                      dcAlpha, delayR_);

            if (++write_ >= kChorusLen) write_ = 0;

            tremPhase_ += tremPhaseInc;
            if (tremPhase_ >= 1.0f) tremPhase_ -= 1.0f;

            chorusPhaseL_ += chorusPhaseInc;
            if (chorusPhaseL_ >= 1.0f) chorusPhaseL_ -= 1.0f;
            chorusPhaseR_ += chorusPhaseInc;
            if (chorusPhaseR_ >= 1.0f) chorusPhaseR_ -= 1.0f;
        }
    }

    ParameterMetadata getParameterMetadata(int idx) override
    {
        switch (idx) {
            case 0: return {0.0f, 1.0f, 0.55f}; // Tone / Cup
            case 1: return {0.0f, 1.0f, 0.48f}; // Reed / Drive
            case 2: return {0.0f, 1.0f, 0.45f}; // Waa (expression-driven)
            default: return {0.0f, 1.0f, 0.5f};
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx) {
            case 0: tone_ = value; break;
            case 1: reed_ = value; break;
            case 2: waa_  = value; break;
            default: break;
        }
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress)) {
            bypassed_ = !bypassed_;
            if (!bypassed_) clearState();
        } else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold)) {
            mode_ = (mode_ == HarmMode::kCupped) ? HarmMode::kOpen : HarmMode::kCupped;
            clearState();
        }
    }

    Color getStateLedColor() override
    {
        if (mode_ == HarmMode::kCupped) {
            return bypassed_ ? Color::kDimYellow : Color::kLightYellow;
        }
        return bypassed_ ? Color::kDimBlue : Color::kLightBlueColor;
    }

private:
    float processChannel(int ch, float x,
                         float preHpAlpha, float shelfLpAlpha, float lowShelfGain,
                         float f1, float q1, float bp1Gain,
                         float f2, float q2, float bp2Gain, float form2Mix,
                         float splitLpAlpha, float baseDrive, float edgeGain,
                         float clipGain, float asymmetry,
                         float postLpAlpha,
                         float tremDepth, float tremPhase,
                         float chorusDepth, float chorusCenter, float chorusPhase,
                         float chorusDry, float chorusWet,
                         float dcAlpha, float* delayBuf)
    {
        // [A] Pre-HPF (1-pole differentiator form: y = alpha*(yPrev + x - xPrev))
        const float hpOut = preHpAlpha * (hpPrev_[ch] + x - inputPrev_[ch]);
        inputPrev_[ch] = x;
        hpPrev_[ch]    = hpOut;

        // [B] Low-shelf split: attenuate the low branch, rejoin with upper branch.
        lowLp_[ch] += shelfLpAlpha * (hpOut - lowLp_[ch]);
        const float lowBranch  = lowLp_[ch] * lowShelfGain;
        const float highBranch = hpOut - lowLp_[ch];
        const float shelved    = lowBranch + highBranch;

        // [C] Formant-1: swept Chamberlin bandpass (hand-cup resonance).
        lowF1_[ch] += f1 * bandF1_[ch];
        const float hiF1 = shelved - lowF1_[ch] - q1 * bandF1_[ch];
        bandF1_[ch] += f1 * hiF1;
        const float formant1 = bandF1_[ch] * bp1Gain;

        // [D] Formant-2: fixed Chamberlin bandpass (nasal/reed-plate resonance).
        lowF2_[ch] += f2 * bandF2_[ch];
        const float hiF2 = shelved - lowF2_[ch] - q2 * bandF2_[ch];
        bandF2_[ch] += f2 * hiF2;
        const float formant2 = bandF2_[ch] * bp2Gain;

        const float voiced = formant1 + form2Mix * formant2;

        // [E] Asymmetric reed saturation, body/edge split.
        splitLp_[ch] += splitLpAlpha * (voiced - splitLp_[ch]);
        const float body = splitLp_[ch];
        const float edge = voiced - body;
        const float drive = body * baseDrive + edge * edgeGain;
        const float clipped = (drive >= 0.0f)
            ? tanhf(drive * clipGain)
            : tanhf(drive * clipGain * asymmetry);

        // [F] Post-LPF (Green Bullet rolloff).
        postLp_[ch] += postLpAlpha * (clipped - postLp_[ch]);
        float y = postLp_[ch];

        // [G] Tremolo AM (dual-reed beat).
        const float trem = 1.0f + tremDepth * sinf(tremPhase * kTwoPi);
        y *= trem;

        // [H] Micro-chorus (reed detune) — fractional-delay read, write the
        // post-tremolo signal, linear interpolation.
        delayBuf[write_] = y;
        float readPos = static_cast<float>(write_) - chorusCenter
                      - chorusDepth * sinf(chorusPhase * kTwoPi);
        if (readPos < 0.0f) readPos += static_cast<float>(kChorusLen);
        const int idx0 = static_cast<int>(readPos) % kChorusLen;
        const int idx1 = (idx0 + 1) % kChorusLen;
        const float frac = readPos - static_cast<float>(static_cast<int>(readPos));
        const float wet = delayBuf[idx0] * (1.0f - frac) + delayBuf[idx1] * frac;
        y = chorusDry * y + chorusWet * wet;

        // [I] DC blocker (scrubs DC offset from asymmetric clipper).
        const float dcOut = dcAlpha * (dcPrev_[ch] + y - dcInputPrev_[ch]);
        dcInputPrev_[ch] = y;
        dcPrev_[ch]      = dcOut;

        // [J] Safety soft-limit.
        return softLimit(dcOut);
    }

    void clearState()
    {
        for (int ch = 0; ch < 2; ++ch) {
            hpPrev_[ch]       = 0.0f;
            inputPrev_[ch]    = 0.0f;
            lowLp_[ch]        = 0.0f;
            lowF1_[ch]        = 0.0f;
            bandF1_[ch]       = 0.0f;
            lowF2_[ch]        = 0.0f;
            bandF2_[ch]       = 0.0f;
            splitLp_[ch]      = 0.0f;
            postLp_[ch]       = 0.0f;
            dcPrev_[ch]       = 0.0f;
            dcInputPrev_[ch]  = 0.0f;
        }
        if (delayL_ && delayR_) {
            for (int i = 0; i < kChorusLen; ++i) {
                delayL_[i] = 0.0f;
                delayR_[i] = 0.0f;
            }
        }
        tremPhase_    = 0.0f;
        chorusPhaseL_ = 0.0f;
        chorusPhaseR_ = 0.25f;
        write_        = 0;
    }

    // --- Knob state ---
    float tone_ = 0.55f;
    float reed_ = 0.48f;
    float waa_  = 0.45f;

    // --- Mode / bypass ---
    HarmMode mode_    = HarmMode::kCupped;
    bool     bypassed_ = false;

    // --- Per-channel filter state ---
    float hpPrev_[2]      = {0.0f, 0.0f};
    float inputPrev_[2]   = {0.0f, 0.0f};
    float lowLp_[2]       = {0.0f, 0.0f};
    float lowF1_[2]       = {0.0f, 0.0f};
    float bandF1_[2]      = {0.0f, 0.0f};
    float lowF2_[2]       = {0.0f, 0.0f};
    float bandF2_[2]      = {0.0f, 0.0f};
    float splitLp_[2]     = {0.0f, 0.0f};
    float postLp_[2]      = {0.0f, 0.0f};
    float dcPrev_[2]      = {0.0f, 0.0f};
    float dcInputPrev_[2] = {0.0f, 0.0f};

    // --- LFO phases & shared delay-line write index ---
    float tremPhase_    = 0.0f;
    float chorusPhaseL_ = 0.0f;
    float chorusPhaseR_ = 0.25f;
    int   write_        = 0;

    // --- Chorus delay lines (live in the working buffer / external RAM) ---
    float* delayL_ = nullptr;
    float* delayR_ = nullptr;
};

// Singleton required by internal/PatchCppWrapper.cpp.
Patch* Patch::getInstance()
{
    static HarmonicaPatch instance;
    return &instance;
}
