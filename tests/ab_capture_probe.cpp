// tests/ab_capture_probe.cpp
//
// Generic single-effect capture tool: compiles against whichever
// effects/<name>.cpp is linked in at build time -- exactly like
// tests/effect_probe.cpp -- runs it under caller-specified knob values,
// hold-mode, and signal choice, and writes a raw float32 (left channel)
// capture + JSON sidecar. Same capture-file convention
// tests/oversample_alias_probe.cpp and tests/effect_probe.cpp's spectral
// capture already use.
//
// This generalizes the *pattern* tests/dimension_chorus_acceptance_test.cpp
// hardcoded via #include-ing one specific effect -- that test stays as a
// cheap, already-verified regression guard for one specific, already-proven
// property. Future A/B comparisons (pre/post-refactor checks, new
// sibling-effect differentiation checks) should build this tool against
// the effect(s) in question via scripts/ab_compare.py instead of writing
// another one-off #include-based test.
//
// Usage:
//   g++ -std=c++20 -O2 -fno-exceptions -fno-rtti -fsingle-precision-constant
//       -I source tests/ab_capture_probe.cpp effects/<name>.cpp -o ab_probe
//   ab_probe <patch_name> <out_dir> [options]
//
// Options:
//   --label NAME      output file stem under out_dir (default: patch_name)
//   --param0 V        knob 0 value, 0..1 (default: patch's own default)
//   --param1 V        knob 1 value, 0..1 (default: patch's own default)
//   --param2 V        knob 2 value, 0..1 (default: patch's own default)
//   --hold            fire kLeftFootSwitchHold before capture
//   --signal sine|burst   input signal choice (default: sine)
//   --scale V         input amplitude scale (default: 1.0)
//   --settle N        settle samples before the capture window (default: 12000)
//   --capture N       captured samples after settle (default: 16384 -- a
//                     power of two, directly FFT-able by
//                     scripts/spectral_fft.py without truncation)

#include "../source/Patch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr int kSampleRate = Patch::kSampleRate;
constexpr int kBlockSize = 128;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPi = 3.14159265358979323846;
constexpr double kSineFreq = 220.0;

std::vector<float> makeSineSignal(int totalSamples, float scale)
{
    std::vector<float> signal(totalSamples, 0.0f);
    for (int i = 0; i < totalSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        signal[i] = static_cast<float>(scale * 0.25 * std::sin(kTwoPi * kSineFreq * t));
    }
    return signal;
}

// Multi-tone envelope, matching tests/effect_probe.cpp's makeBurstInput
// shape (a realistic, harmonically rich test signal, not a plain tone).
std::vector<float> makeBurstSignal(int totalSamples, float scale)
{
    std::vector<float> signal(totalSamples, 0.0f);
    for (int i = 0; i < totalSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        const double envelope = 0.78 + 0.22 * std::sin(2.0 * kPi * 1.7 * t);
        const double composite =
          0.18 * std::sin(kTwoPi * 110.0 * t) +
          0.12 * std::sin(kTwoPi * 220.0 * t + 0.3) +
          0.08 * std::sin(kTwoPi * 330.0 * t + 0.6) +
          0.04 * std::sin(kTwoPi * 440.0 * t + 0.9);
        signal[i] = static_cast<float>(scale * envelope * composite);
    }
    return signal;
}

float parseFloatArg(const char* value)
{
    return std::strtof(value, nullptr);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: %s <patch_name> <out_dir> [--label NAME] "
                     "[--param0 V] [--param1 V] [--param2 V] [--hold] "
                     "[--signal sine|burst] [--scale V] [--settle N] [--capture N]\n",
                     argv[0]);
        return EXIT_FAILURE;
    }

    const std::string patchName = argv[1];
    const std::string outDir = argv[2];

    std::string label = patchName;
    bool paramOverride[3] = {false, false, false};
    float paramValue[3] = {0.0f, 0.0f, 0.0f};
    bool hold = false;
    std::string signalKind = "sine";
    float scale = 1.0f;
    int settleSamples = 12000;
    int captureSamples = 16384;

    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        auto nextValue = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires a value\n", arg.c_str());
                std::exit(EXIT_FAILURE);
            }
            return argv[++i];
        };

        if (arg == "--label") {
            label = nextValue();
        } else if (arg == "--param0") {
            paramOverride[0] = true;
            paramValue[0] = parseFloatArg(nextValue());
        } else if (arg == "--param1") {
            paramOverride[1] = true;
            paramValue[1] = parseFloatArg(nextValue());
        } else if (arg == "--param2") {
            paramOverride[2] = true;
            paramValue[2] = parseFloatArg(nextValue());
        } else if (arg == "--hold") {
            hold = true;
        } else if (arg == "--signal") {
            signalKind = nextValue();
        } else if (arg == "--scale") {
            scale = parseFloatArg(nextValue());
        } else if (arg == "--settle") {
            settleSamples = std::atoi(nextValue());
        } else if (arg == "--capture") {
            captureSamples = std::atoi(nextValue());
        } else {
            std::fprintf(stderr, "error: unknown option: %s\n", arg.c_str());
            return EXIT_FAILURE;
        }
    }

    if (signalKind != "sine" && signalKind != "burst") {
        std::fprintf(stderr, "error: --signal must be 'sine' or 'burst'\n");
        return EXIT_FAILURE;
    }

    Patch* patch = Patch::getInstance();
    std::vector<float> workingBuffer(Patch::kWorkingBufferSize, 0.0f);
    patch->setWorkingBuffer(std::span<float, Patch::kWorkingBufferSize>(
      workingBuffer.data(), Patch::kWorkingBufferSize));
    patch->init();

    float params[3];
    for (int idx = 0; idx < 3; ++idx) {
        params[idx] = paramOverride[idx] ? paramValue[idx]
                                         : patch->getParameterMetadata(idx).defaultValue;
        patch->setParamValue(idx, params[idx]);
    }

    if (hold) {
        patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
    }

    const int totalSamples = settleSamples + captureSamples;
    const std::vector<float> input = signalKind == "sine"
      ? makeSineSignal(totalSamples, scale)
      : makeBurstSignal(totalSamples, scale);

    std::vector<float> outLeft(totalSamples, 0.0f);
    std::vector<float> outRight(totalSamples, 0.0f);
    std::vector<float> leftBlock(kBlockSize, 0.0f);
    std::vector<float> rightBlock(kBlockSize, 0.0f);

    for (int offset = 0; offset < totalSamples; offset += kBlockSize) {
        const int blockSize = std::min(kBlockSize, totalSamples - offset);
        std::copy_n(input.begin() + offset, blockSize, leftBlock.begin());
        std::copy_n(input.begin() + offset, blockSize, rightBlock.begin());

        patch->processAudio(std::span<float>(leftBlock.data(), static_cast<size_t>(blockSize)),
                            std::span<float>(rightBlock.data(), static_cast<size_t>(blockSize)));

        std::copy_n(leftBlock.begin(), blockSize, outLeft.begin() + offset);
        std::copy_n(rightBlock.begin(), blockSize, outRight.begin() + offset);
    }

    // Discard the settle prefix -- only the steady-state window is captured.
    const std::vector<float> capture(outLeft.begin() + settleSamples, outLeft.end());

    std::filesystem::create_directories(outDir);
    const std::string binPath = outDir + "/" + label + ".f32";
    const std::string jsonPath = outDir + "/" + label + ".json";

    FILE* bin = std::fopen(binPath.c_str(), "wb");
    if (!bin) {
        std::fprintf(stderr, "cannot open %s\n", binPath.c_str());
        return EXIT_FAILURE;
    }
    std::fwrite(capture.data(), sizeof(float), capture.size(), bin);
    std::fclose(bin);

    FILE* js = std::fopen(jsonPath.c_str(), "w");
    if (!js) {
        std::fprintf(stderr, "cannot open %s\n", jsonPath.c_str());
        return EXIT_FAILURE;
    }
    std::fprintf(js,
                 "{\"patch\":\"%s\",\"label\":\"%s\",\"sample_rate\":%d,\"n\":%zu,"
                 "\"params\":[%.6f,%.6f,%.6f],\"hold\":%s,\"signal_kind\":\"%s\","
                 "\"scale\":%.6f,\"settle_samples\":%d}\n",
                 patchName.c_str(), label.c_str(), kSampleRate, capture.size(),
                 static_cast<double>(params[0]), static_cast<double>(params[1]),
                 static_cast<double>(params[2]), hold ? "true" : "false",
                 signalKind.c_str(), static_cast<double>(scale), settleSamples);
    std::fclose(js);

    std::printf("wrote %s (%zu samples) + %s\n", binPath.c_str(), capture.size(),
               jsonPath.c_str());
    return EXIT_SUCCESS;
}
