// Host-side behavioral acceptance checks for Funk Machine.
//
// This test includes the effect directly so it exercises the actual Patch
// implementation without introducing a second Patch::getInstance() definition.
// It checks properties that are meaningful without pretending to replace
// hardware listening.

#include "../effects/funk_machine_envelope_filter.cpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
constexpr size_t kFrames = 4096;
constexpr float kPi = 3.14159265f;
// kFs is NOT redefined here -- funk_machine_envelope_filter.cpp's #include
// above already brings a `constexpr float kFs` into this translation
// unit's merged anonymous namespace, and a second definition of the same
// name in the same namespace is a hard redefinition error, not a shadow.
// (This bug was present in the original acceptance test too -- it had
// never actually been compiled before this revision; see the git history
// of this file.)

struct RunResult
{
    float gainRms;
    float peak;
};

RunResult runTone(Patch* patch, float amplitude, float hz)
{
    std::array<float, kFrames> left{};
    std::array<float, kFrames> right{};

    float inputSumSq = 0.0f;
    for (size_t i = 0; i < kFrames; ++i) {
        const float x = amplitude * sinf(2.0f * kPi * hz * static_cast<float>(i) / kFs);
        left[i] = x;
        right[i] = x;
        inputSumSq += x * x;
    }

    patch->processAudio(left, right);

    float outputSumSq = 0.0f;
    float peak = 0.0f;
    for (size_t i = 0; i < kFrames; ++i) {
        assert(std::isfinite(left[i]));
        assert(std::isfinite(right[i]));
        peak = std::max(peak, fabsf(left[i]));
        peak = std::max(peak, fabsf(right[i]));
        outputSumSq += left[i] * left[i];
    }

    return {sqrtf(outputSumSq / inputSumSq), peak};
}
}

int main()
{
    Patch* patch = Patch::getInstance();
    patch->init();

    // Defaults must be normalized and usable.
    for (int idx = 0; idx < 3; ++idx) {
        const auto meta = patch->getParameterMetadata(idx);
        assert(meta.minValue == 0.0f);
        assert(meta.maxValue == 1.0f);
        assert(meta.defaultValue >= 0.0f && meta.defaultValue <= 1.0f);
    }

    // Output must remain finite and bounded under a strong nominal input.
    const auto nominal = runTone(patch, 0.85f, 220.0f);
    assert(nominal.peak < 1.0f);

    // Envelope behavior must be level-dependent. Compare output/input RMS ratio,
    // not raw RMS, so a louder stimulus does not pass merely because it is louder.
    patch->init();
    patch->setParamValue(0, 0.75f); // stronger sensitivity
    patch->setParamValue(1, 0.65f); // vocal resonance
    patch->setParamValue(2, 0.20f); // low-biased sweep
    const auto quiet = runTone(patch, 0.05f, 500.0f);

    patch->init();
    patch->setParamValue(0, 0.75f);
    patch->setParamValue(1, 0.65f);
    patch->setParamValue(2, 0.20f);
    const auto loud = runTone(patch, 0.60f, 500.0f);

    // A touch-sensitive filter should not behave as a fixed linear filter across
    // these two input levels. Leave tolerance broad; this protects the concept,
    // not a fragile exact voicing number.
    assert(fabsf(loud.gainRms - quiet.gainRms) > 0.05f);

    // Hold must select a visibly distinct Guitar/Keys voice.
    patch->init();
    const auto bassColor = patch->getStateLedColor();
    patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
    const auto keysColor = patch->getStateLedColor();
    assert(bassColor != keysColor);

    // Bypass must be exact pass-through.
    patch->init();
    patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchPress));
    std::array<float, 32> left{};
    std::array<float, 32> right{};
    std::array<float, 32> reference{};
    for (size_t i = 0; i < left.size(); ++i) {
        reference[i] = 0.4f * sinf(0.31f * static_cast<float>(i));
        left[i] = reference[i];
        right[i] = reference[i];
    }
    patch->processAudio(left, right);
    for (size_t i = 0; i < left.size(); ++i) {
        assert(left[i] == reference[i]);
        assert(right[i] == reference[i]);
    }

    // Up mode must be a real, distinct inversion of Down -- not just a
    // measurable difference, but the opposite quiet/loud trend at the same
    // test point. From the power-on default (Bass-Down), 3 Hold presses
    // reach Bass-Up (Bass-Down -> GuitarKeys-Down -> GuitarKeys-Up ->
    // Bass-Up), isolating direction from voice for an apples-to-apples
    // comparison against the quiet/loud pair measured above (which ran in
    // the default Bass-Down state).
    patch->init();
    patch->setParamValue(0, 0.75f);
    patch->setParamValue(1, 0.65f);
    patch->setParamValue(2, 0.20f);
    for (int i = 0; i < 3; ++i) {
        patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
    }
    const auto quietUp = runTone(patch, 0.05f, 500.0f);

    patch->init();
    patch->setParamValue(0, 0.75f);
    patch->setParamValue(1, 0.65f);
    patch->setParamValue(2, 0.20f);
    for (int i = 0; i < 3; ++i) {
        patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
    }
    const auto loudUp = runTone(patch, 0.60f, 500.0f);

    const float deltaDown = loud.gainRms - quiet.gainRms;
    const float deltaUp   = loudUp.gainRms - quietUp.gainRms;
    assert(fabsf(deltaUp) > 0.05f);
    // Down and Up must trend in opposite directions as input gets louder --
    // that is the entire point of the inversion. A same-sign delta would
    // mean Up isn't actually inverting anything.
    assert((deltaDown > 0.0f) != (deltaUp > 0.0f));

    // Linked-stereo detector: the envelope must be driven by the hotter
    // channel, not each channel independently. Hold R's content fixed and
    // quiet; only change whether L is quiet (matching R) or loud. If the
    // detector is genuinely linked, R's own processing should differ
    // between these two cases even though R's input never changes --
    // proof that R is being filtered under an envelope L controls, not one
    // R computes independently (which would make R identical in both cases).
    auto runDivergentLR = [](Patch* p, float lAmplitude, float rAmplitude, float hz) {
        std::array<float, kFrames> left{};
        std::array<float, kFrames> right{};
        float rInputSumSq = 0.0f;
        for (size_t i = 0; i < kFrames; ++i) {
            const float phase = 2.0f * kPi * hz * static_cast<float>(i) / kFs;
            left[i]  = lAmplitude * sinf(phase);
            right[i] = rAmplitude * sinf(phase);
            rInputSumSq += right[i] * right[i];
        }
        p->processAudio(left, right);
        float rOutputSumSq = 0.0f;
        for (size_t i = 0; i < kFrames; ++i) {
            assert(std::isfinite(left[i]));
            assert(std::isfinite(right[i]));
            rOutputSumSq += right[i] * right[i];
        }
        return sqrtf(rOutputSumSq / rInputSumSq);
    };

    patch->init();
    patch->setParamValue(0, 0.75f);
    patch->setParamValue(1, 0.65f);
    patch->setParamValue(2, 0.20f);
    const float rGainBothQuiet = runDivergentLR(patch, 0.05f, 0.05f, 500.0f);

    patch->init();
    patch->setParamValue(0, 0.75f);
    patch->setParamValue(1, 0.65f);
    patch->setParamValue(2, 0.20f);
    const float rGainLoudL = runDivergentLR(patch, 0.60f, 0.05f, 500.0f);

    assert(fabsf(rGainLoudL - rGainBothQuiet) > 0.05f);

    // Knob-jump / click safety: this patch's control-rate coefficient
    // update is deliberately un-smoothed (see the walkthrough doc's
    // Decision 5), so this is the only place that risk gets any automated
    // coverage. Jump Bias from one extreme to the other mid-stream, at max
    // resonance (the theoretically riskiest condition per the SVF's
    // stability margin), and confirm output stays finite and bounded
    // through the transition. This can't assert "sounds click-free," but
    // it catches a NaN/blowup that a purely static per-scenario test would
    // never see.
    patch->init();
    patch->setParamValue(0, 0.75f);
    patch->setParamValue(1, 1.0f);  // max resonance (Q ~= 7.5)
    patch->setParamValue(2, 0.0f);  // Bias at one extreme
    {
        std::array<float, 512> jumpLeft{};
        std::array<float, 512> jumpRight{};
        for (size_t i = 0; i < jumpLeft.size(); ++i) {
            const float x = 0.7f * sinf(2.0f * kPi * 500.0f * static_cast<float>(i) / kFs);
            jumpLeft[i]  = x;
            jumpRight[i] = x;
        }
        patch->processAudio(jumpLeft, jumpRight);
        patch->setParamValue(2, 1.0f);  // jump Bias to the opposite extreme, mid-stream
        patch->processAudio(jumpLeft, jumpRight);
        for (size_t i = 0; i < jumpLeft.size(); ++i) {
            assert(std::isfinite(jumpLeft[i]));
            assert(std::isfinite(jumpRight[i]));
            assert(fabsf(jumpLeft[i]) < 1.0f);
            assert(fabsf(jumpRight[i]) < 1.0f);
        }
    }

    std::printf("Funk Machine acceptance test PASS\n");
    return 0;
}
