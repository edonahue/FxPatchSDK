// Optical Compressor — three selectable compression circuits
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
//         sdk/Patch.h           (modified SDK with isParamEnabled / ParamSource)
//         wdf/WdfOpticalCircuits.h  (PC2ACircuit, OpticalLevelerCircuit, HybridOptOtaCircuit)
//
//       isParamEnabled(int, ParamSource) is an extension not in the base SDK.
//
// Controls:
//   Left knob  (0): Peak Reduction / Sensitivity
//   Mid knob   (1): Gain (makeup gain)
//   Right knob (2): Circuit — 0–0.33: PC-2A  |  0.33–0.66: Leveler  |  0.66–1.0: Hybrid Opt/OTA
//   Expression (0): HF Emphasis (0=flat sidechain, 1=treble-focused)
//
// Footswitch:
//   Press: Toggle attack mode  (fast 5ms ↔ slow 50ms)
//   Hold:  Toggle release mode (short 200ms ↔ long 2000ms)
//
// LED:
//   PC-2A circuit:   kLightYellow (vintage gold)
//   Leveler circuit: kDimYellow   (simpler, dimmer)
//   Hybrid circuit:  kDimCyan     (modern hybrid)
//   Heavy compression (high LDR inertia): dims toward kDimYellow / kDimBlue
//   Fast attack mode: overrides to kDimWhite (brighter, crisper look)

#include "sdk/Patch.h"
#include "wdf/WdfOpticalCircuits.h"
#include <algorithm>
#include <cmath>

class PatchImpl : public Patch
{
public:
    void init() override
    {
        float sr = static_cast<float>(kSampleRate);

        pc2a_.init(sr);
        pc2a_.setPeakReduction(peakReduction_);
        pc2a_.setGain(gain_);
        pc2a_.setHFEmphasis(hfEmphasis_);

        leveler_.init(sr);
        leveler_.setThreshold(peakReduction_);

        hybrid_.init(sr);
        hybrid_.setSensitivity(peakReduction_);
        hybrid_.setMakeupGain(gain_);
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
            float inL = *leftIt;
            float inR = *rightIt;
            float outL = 0.0f;
            float outR = 0.0f;

            if (circuit_ == 0) {
                // PC-2A: linked stereo compression
                pc2a_.processStereo(inL, inR, outL, outR);
            } else if (circuit_ == 1) {
                // Optical Leveler: process each channel independently with same threshold
                outL = leveler_.process(inL);
                outR = leveler_.process(inR);
            } else {
                // Hybrid Opt/OTA: process each channel independently
                outL = hybrid_.process(inL);
                outR = hybrid_.process(inR);
            }

            *leftIt  = outL;
            *rightIt = outR;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (paramIdx)
        {
            case static_cast<int>(endless::ParamId::kParamLeft):   // Peak Reduction / Sensitivity
                return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
            case static_cast<int>(endless::ParamId::kParamMid):    // Gain (makeup)
                return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
            case static_cast<int>(endless::ParamId::kParamRight):  // Circuit select
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
            default:
                return ParameterMetadata{ 0.0f, 1.0f, 0.0f };
        }
    }

    bool isParamEnabled(int paramIdx, ParamSource source) override
    {
        if (source == ParamSource::kParamSourceExpression && paramIdx == 0) {
            return true;  // Expression pedal controls HF Emphasis (idx 0)
        }
        if (source == ParamSource::kParamSourceKnob) {
            return paramIdx >= 0 && paramIdx <= 2;
        }
        return false;
    }

    void setParamValue(int paramIdx, float value) override
    {
        switch (paramIdx)
        {
            case static_cast<int>(endless::ParamId::kParamLeft):
                peakReduction_ = value;
                pc2a_.setPeakReduction(value);
                leveler_.setThreshold(value);
                hybrid_.setSensitivity(value);
                break;

            case static_cast<int>(endless::ParamId::kParamMid):
                gain_ = value;
                pc2a_.setGain(value);
                hybrid_.setMakeupGain(value);
                break;

            case static_cast<int>(endless::ParamId::kParamRight):
                if (value < 0.33f) {
                    circuit_ = 0;
                } else if (value < 0.66f) {
                    circuit_ = 1;
                } else {
                    circuit_ = 2;
                }
                break;

            default:
                break;
        }
    }

    void handleAction(int actionIdx) override
    {
        switch (actionIdx)
        {
            case static_cast<int>(endless::ActionId::kLeftFootSwitchPress):
                fastAttack_ = !fastAttack_;
                applyTimeConstants();
                break;

            case static_cast<int>(endless::ActionId::kLeftFootSwitchHold):
                longRelease_ = !longRelease_;
                applyTimeConstants();
                break;

            default:
                break;
        }
    }

    Color getStateLedColor() override
    {
        Color base;
        float inertia = 0.0f;

        if (circuit_ == 0) {
            base    = Color::kLightYellow;
            inertia = pc2a_.getLdrInertia();
        } else if (circuit_ == 1) {
            base    = Color::kDimYellow;
            inertia = 0.5f;
        } else {
            base    = Color::kDimCyan;
            inertia = 0.5f;
        }

        if (fastAttack_) {
            return Color::kDimWhite;
        }

        if (inertia > 0.5f) {
            if (circuit_ == 0) return Color::kDimYellow;
            if (circuit_ == 2) return Color::kDimBlue;
        }

        return base;
    }

private:
    PC2ACircuit           pc2a_;
    OpticalLevelerCircuit leveler_;
    HybridOptOtaCircuit   hybrid_;

    int   circuit_       = 0;
    float peakReduction_ = 0.5f;
    float gain_          = 0.5f;
    float hfEmphasis_    = 0.0f;
    bool  fastAttack_    = false;
    bool  longRelease_   = false;

    void applyTimeConstants() noexcept
    {
        float attackVal  = fastAttack_  ? 0.0f : 1.0f;
        float releaseVal = longRelease_ ? 1.0f : 0.0f;
        hybrid_.setAttack(attackVal);
        hybrid_.setRelease(releaseVal);
    }
};

Patch* Patch::getInstance()
{
    static PatchImpl instance;
    return &instance;
}
