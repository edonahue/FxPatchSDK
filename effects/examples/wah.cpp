// Cry Baby Wah — resonant LC filter with auto-wah, LFO, and envelope modes
//
// Source: sthompsonjr/Endless-FxPatchSDK
//         https://github.com/sthompsonjr/Endless-FxPatchSDK
//         MIT License
//
// NOTE: This file depends on WDF (Wave Digital Filter) circuit headers and a
//       modified SDK layout from sthompsonjr's fork. It will NOT compile
//       directly against the stock polyend/FxPatchSDK without those dependencies.
//       It is included here as a reference/study copy.
//
//       Required from sthompsonjr's repo:
//         sdk/Patch.h              (modified SDK with isParamEnabled / ParamSource)
//         wdf/WdfWahCircuit.h      (AutoWahCircuit and AutoWahCircuit::Mode)
//         dsp/ParameterSmoother.h  (ParameterSmoother)
//
//       isParamEnabled(int, ParamSource) is an extension not in the base SDK.
//
// Controls:
//   Left knob  (0): Resonance  — 0.0=gentle Q (3–6), 1.0=self-oscillating (Q 6–20+)
//   Mid knob   (1): Sensitivity — auto-wah envelope sensitivity
//   Right knob (2): Mode — 4 positions:
//                     0.00–0.25: Expression (expression pedal sweeps wah)
//                     0.25–0.50: Auto-wah  (envelope follower drives sweep)
//                     0.50–0.75: LFO wah   (slow rhythmic sweep)
//                     0.75–1.00: EnvLfo    (envelope controls LFO rate)
//   Expression (0): Sweep position — 0.0=heel (~350 Hz), 1.0=toe (~2.2 kHz)
//                   Active in Expression and EnvLfo modes
//
// Footswitch:
//   Press: Toggle sweep direction (heel=low freq / heel=high freq)
//   Hold:  Tap tempo for LFO wah rate
//
// LED:
//   Expression mode:  kDimYellow  (classic wah — warm)
//   Auto-wah:         kDimGreen   (envelope = organic)
//   LFO wah:          kDimCyan    (rhythmic = cool)
//   EnvLfo:           kMagenta    (hybrid)
//   resonance > 0.8:  kDarkRed    (oscillation warning / feature)

#include "sdk/Patch.h"
#include "wdf/WdfWahCircuit.h"
#include "dsp/ParameterSmoother.h"
#include <cmath>

class PatchImpl : public Patch
{
  public:
    void init() override
    {
        wah_.init(static_cast<float>(kSampleRate));
        wah_.setOutputLevel(1.0f);
    }

    void setWorkingBuffer(std::span<float, kWorkingBufferSize> buffer) override
    {
        (void)buffer;
    }

    void processAudio(std::span<float> audioBufferLeft,
                      std::span<float> audioBufferRight) override
    {
        for (auto leftIt = audioBufferLeft.begin(), rightIt = audioBufferRight.begin();
             leftIt != audioBufferLeft.end();
             ++leftIt, ++rightIt)
        {
            float sweepIn = sweepReversed_ ? 1.0f - currentSweep_ : currentSweep_;
            float out = wah_.process(*leftIt, sweepIn);
            *leftIt  = out;
            *rightIt = out;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (paramIdx)
        {
            case 0: return ParameterMetadata{ 0.0f, 1.0f, 0.0f }; // Resonance
            case 1: return ParameterMetadata{ 0.0f, 1.0f, 0.5f }; // Sensitivity
            case 2: return ParameterMetadata{ 0.0f, 1.0f, 0.0f }; // Mode
            case 3: return ParameterMetadata{ 0.0f, 1.0f, 0.5f }; // Sweep (expression)
            default: return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx)
        {
            case 0: // Resonance
                currentResonance_ = value;
                wah_.setResonance(value);
                break;

            case 1: // Sensitivity
                wah_.setSensitivity(value);
                break;

            case 2: // Mode
                currentMode_ = modeFromKnob(value);
                wah_.setMode(currentMode_);
                break;

            case 3: // Sweep (expression pedal)
                currentSweep_ = value;
                break;

            default:
                break;
        }
    }

    bool isParamEnabled(int paramIdx, ParamSource source) override
    {
        switch (source)
        {
            case ParamSource::kParamSourceKnob:
                return paramIdx < 3;
            case ParamSource::kParamSourceExpression:
                return paramIdx == 3;
            default:
                return false;
        }
    }

    void handleAction(int idx) override
    {
        if (idx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress))
        {
            sweepReversed_ = !sweepReversed_;
        }
        else if (idx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold))
        {
            // Tap tempo for LFO rate
            if (lastHoldSample_ > 0)
            {
                int elapsed = holdSampleCounter_ - lastHoldSample_;
                if (elapsed > 2400 && elapsed < 240000) // 50ms–5s range
                {
                    float periodSec = static_cast<float>(elapsed) / static_cast<float>(kSampleRate);
                    float hz = 1.0f / periodSec;
                    // Inverse log taper: hz = 0.05 * 100^rate
                    float rate = logf(hz / 0.05f) / logf(100.0f);
                    rate = std::clamp(rate, 0.0f, 1.0f);
                    wah_.setRate(rate);
                }
            }
            lastHoldSample_ = holdSampleCounter_;
        }
        holdSampleCounter_++;
    }

    Color getStateLedColor() override
    {
        // High resonance → oscillation warning overrides mode color
        if (currentResonance_ > 0.8f)
            return Color::kDarkRed;

        switch (currentMode_)
        {
            case AutoWahCircuit::Mode::Expression: return Color::kDimYellow;
            case AutoWahCircuit::Mode::Envelope:   return Color::kDimGreen;
            case AutoWahCircuit::Mode::Lfo:        return Color::kDimCyan;
            case AutoWahCircuit::Mode::EnvLfo:     return Color::kMagenta;
            default:                               return Color::kDimYellow;
        }
    }

  private:
    static AutoWahCircuit::Mode modeFromKnob(float v)
    {
        if (v < 0.25f) return AutoWahCircuit::Mode::Expression;
        if (v < 0.50f) return AutoWahCircuit::Mode::Envelope;
        if (v < 0.75f) return AutoWahCircuit::Mode::Lfo;
        return AutoWahCircuit::Mode::EnvLfo;
    }

    AutoWahCircuit wah_;

    float currentSweep_     = 0.5f;
    float currentResonance_ = 0.0f;
    bool  sweepReversed_    = false;

    AutoWahCircuit::Mode currentMode_ = AutoWahCircuit::Mode::Expression;

    // Tap tempo state (sample counter)
    int holdSampleCounter_ = 0;
    int lastHoldSample_    = 0;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}
