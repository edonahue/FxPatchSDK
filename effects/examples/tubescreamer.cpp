// Tubescreamer Family — TS808, TS9, and Klon circuit variants
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
//         sdk/Patch.h                  (modified SDK with isParamEnabled / ParamSource)
//         wdf/WdfTubescreamerCircuit.h  (TS808Circuit, TS9Circuit, KlonClipStage)
//         dsp/ParameterSmoother.h      (ParameterSmoother)
//         dsp/Saturation.h             (sat::softClip)
//
//       isParamEnabled(int, ParamSource) is an extension not in the base SDK.
//
// Controls:
//   Left knob  (0): Drive   — 0.0=clean boost, 1.0=full overdrive
//   Mid knob   (1): Tone    — 0.0=dark, 1.0=bright
//   Right knob (2): Circuit — 0–0.33=TS808, 0.33–0.66=TS9, 0.66–1.0=Klon
//   Expression (0): Level   — real-time output volume
//
// Footswitch:
//   Press: Toggle Age mode      (vintage worn op-amp character)
//   Hold:  Toggle Asymmetry mode (even-harmonic content / clarity shift)
//
// LED:
//   TS808: kDimGreen  |  TS9: kLightGreen  |  Klon: kDimYellow
//   Age on + Asym on: kBeige
//   Age on only:      kDarkCobalt
//   Asym on only:     kDarkRed

#include "sdk/Patch.h"
#include "wdf/WdfTubescreamerCircuit.h"
#include "dsp/ParameterSmoother.h"
#include "dsp/Saturation.h"
#include <cmath>

class PatchImpl : public Patch
{
public:
    void init() override
    {
        ts808_.init(static_cast<float>(kSampleRate));
        ts808_.setDrive(0.5f);
        ts808_.setTone(0.5f);
        ts808_.setLevel(0.7f);

        ts9_.init(static_cast<float>(kSampleRate));
        ts9_.setDrive(0.5f);
        ts9_.setTone(0.5f);
        ts9_.setLevel(0.7f);

        klon_.init(static_cast<float>(kSampleRate));
        klon_.setGain(0.5f);

        levelSmoother_.init(static_cast<float>(kSampleRate), 20.0f);
        levelSmoother_.snapTo(0.7f);

        circuit_ = Circuit::TS808;
        ageOn_   = false;
        asymOn_  = false;
        drive_   = 0.5f;
        tone_    = 0.5f;
        level_   = 0.7f;
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
            float smoothLevel = levelSmoother_.process();
            float out = 0.0f;

            switch (circuit_)
            {
                case Circuit::TS808:
                    out = ts808_.process(*leftIt);
                    break;
                case Circuit::TS9:
                    out = ts9_.process(*leftIt);
                    break;
                case Circuit::Klon:
                    out = klon_.process(*leftIt);
                    break;
            }

            out *= smoothLevel;
            out = sat::softClip(out);

            *leftIt  = out;
            *rightIt = out;
        }
    }

    ParameterMetadata getParameterMetadata(int paramIdx) override
    {
        switch (paramIdx)
        {
            case 0: return ParameterMetadata{ 0.0f, 1.0f, 0.5f }; // Drive
            case 1: return ParameterMetadata{ 0.0f, 1.0f, 0.5f }; // Tone
            case 2: return ParameterMetadata{ 0.0f, 1.0f, 0.0f }; // Circuit selector
            default: return ParameterMetadata{ 0.0f, 1.0f, 0.5f };
        }
    }

    void setParamValue(int idx, float value) override
    {
        switch (idx)
        {
            case 0:
                drive_ = value;
                ts808_.setDrive(value);
                ts9_.setDrive(value);
                klon_.setGain(value);
                break;
            case 1:
                tone_ = value;
                ts808_.setTone(value);
                ts9_.setTone(value);
                break;
            case 2:
            {
                Circuit newCircuit;
                if (value < 0.33f)
                    newCircuit = Circuit::TS808;
                else if (value < 0.66f)
                    newCircuit = Circuit::TS9;
                else
                    newCircuit = Circuit::Klon;

                if (newCircuit != circuit_)
                {
                    circuit_ = newCircuit;
                    applyCurrentParams();
                }
                break;
            }
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
                return paramIdx == 0; // Expression pedal → Level
            default:
                return false;
        }
    }

    void handleAction(int actionIdx) override
    {
        if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchPress))
        {
            ageOn_ = !ageOn_;
            float age = ageOn_ ? 0.6f : 0.0f;
            ts808_.opAmp().setAge(age);
            ts9_.opAmp().setAge(age);
            klon_.opAmp().setAge(age);
        }
        else if (actionIdx == static_cast<int>(endless::ActionId::kLeftFootSwitchHold))
        {
            asymOn_ = !asymOn_;
            float character = asymOn_ ? 0.5f : 0.0f;
            ts808_.opAmp().setCharacter(character);
            ts9_.opAmp().setCharacter(character);
            klon_.opAmp().setCharacter(character);

            float clarityHz = asymOn_ ? 400.0f : 720.0f;
            ts808_.setClarityFreq(clarityHz);
            ts9_.setClarityFreq(clarityHz);
        }
    }

    Color getStateLedColor() override
    {
        Color base;
        switch (circuit_)
        {
            case Circuit::TS808: base = Color::kDimGreen;   break;
            case Circuit::TS9:   base = Color::kLightGreen; break;
            case Circuit::Klon:  base = Color::kDimYellow;  break;
            default:             base = Color::kDimGreen;   break;
        }

        if (ageOn_ && asymOn_)   return Color::kBeige;
        if (ageOn_)              return Color::kDarkCobalt;
        if (asymOn_)             return Color::kDarkRed;
        return base;
    }

private:
    void applyCurrentParams()
    {
        ts808_.setDrive(drive_);
        ts808_.setTone(tone_);
        ts9_.setDrive(drive_);
        ts9_.setTone(tone_);
        klon_.setGain(drive_);
    }

    enum class Circuit { TS808, TS9, Klon };

    TS808Circuit      ts808_;
    TS9Circuit        ts9_;
    KlonClipStage     klon_;
    ParameterSmoother levelSmoother_;

    Circuit circuit_ = Circuit::TS808;
    bool    ageOn_   = false;
    bool    asymOn_  = false;
    float   drive_   = 0.5f;
    float   tone_    = 0.5f;
    float   level_   = 0.7f;
};

static PatchImpl patch;

Patch* Patch::getInstance()
{
    return &patch;
}
