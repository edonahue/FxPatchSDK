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
constexpr float kFs = static_cast<float>(Patch::kSampleRate);

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

    std::printf("Funk Machine acceptance test PASS\n");
    return 0;
}
