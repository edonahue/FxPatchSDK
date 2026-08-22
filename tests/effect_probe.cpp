#include "../source/Patch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
constexpr int kSampleRate = Patch::kSampleRate;
constexpr int kBlockSize = 128;
constexpr int kBurstActiveSamples = kSampleRate;
constexpr int kBurstSilenceSamples = kSampleRate / 2;
constexpr int kBurstTotalSamples = kBurstActiveSamples + kBurstSilenceSamples;
constexpr int kSineSamples = kSampleRate;
constexpr int kSineAnalysisStart = kSampleRate / 4;
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kSineFreq = 220.0;
constexpr double kSineAmp = 0.25;
constexpr std::array<float, 5> kSweepValues = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

struct SignalMetrics
{
    double inputRms = 0.0;
    double outputRms = 0.0;
    double outputMidbandRms = 0.0;
    double deltaRatio = 0.0;
    double correlation = 0.0;
    double fundamentalGain = 0.0;
    double residualRatio = 0.0;
    double tailRms = 0.0;
    double peakAbs = 0.0;
    double hotSampleRatio = 0.0;
    double clipSampleRatio = 0.0;
};

struct ScenarioMetrics
{
    SignalMetrics burst;
    SignalMetrics sine;
    bool nanOrInfDetected = false;
};

double rms(const std::vector<float>& values, std::size_t start, std::size_t end)
{
    if (start >= end || end > values.size()) {
        return 0.0;
    }

    double sum = 0.0;
    for (std::size_t i = start; i < end; ++i) {
        const double sample = static_cast<double>(values[i]);
        sum += sample * sample;
    }
    return std::sqrt(sum / static_cast<double>(end - start));
}

double peakAbs(const std::vector<float>& values, std::size_t start, std::size_t end)
{
    if (start >= end || end > values.size()) {
        return 0.0;
    }

    double peak = 0.0;
    for (std::size_t i = start; i < end; ++i) {
        peak = std::max(peak, std::fabs(static_cast<double>(values[i])));
    }
    return peak;
}

// A NaN/Inf-producing patch would otherwise silently corrupt every
// downstream RMS/ratio metric (NaN propagates through sum += x*x, etc.)
// rather than being flagged. This is the one check that catches it.
bool allFinite(const float* values, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(values[i])) {
            return false;
        }
    }
    return true;
}

double ratioAboveAbsThreshold(const std::vector<float>& values,
                              std::size_t start,
                              std::size_t end,
                              double threshold)
{
    if (start >= end || end > values.size()) {
        return 0.0;
    }

    std::size_t hits = 0;
    for (std::size_t i = start; i < end; ++i) {
        if (std::fabs(static_cast<double>(values[i])) >= threshold) {
            ++hits;
        }
    }
    return static_cast<double>(hits) / static_cast<double>(end - start);
}

double midbandRms(const std::vector<float>& values, std::size_t start, std::size_t end)
{
    if (start >= end || end > values.size()) {
        return 0.0;
    }

    constexpr double kTwoPi = 6.28318530717958647692;
    constexpr double kHpHz = 140.0;
    constexpr double kLpHz = 3200.0;
    const double hpAlpha = 1.0 / (1.0 + kTwoPi * kHpHz / static_cast<double>(kSampleRate));
    const double lpOmega = kTwoPi * kLpHz / static_cast<double>(kSampleRate);
    const double lpAlpha = lpOmega / (1.0 + lpOmega);

    double hpPrev = 0.0;
    double xPrev = 0.0;
    double lpPrev = 0.0;
    double sum = 0.0;

    for (std::size_t i = start; i < end; ++i) {
        const double x = static_cast<double>(values[i]);
        const double hp = hpAlpha * (hpPrev + x - xPrev);
        xPrev = x;
        hpPrev = hp;

        lpPrev += lpAlpha * (hp - lpPrev);
        sum += lpPrev * lpPrev;
    }

    return std::sqrt(sum / static_cast<double>(end - start));
}

double correlation(const std::vector<float>& x,
                   const std::vector<float>& y,
                   std::size_t start,
                   std::size_t end)
{
    if (start >= end || end > x.size() || end > y.size()) {
        return 0.0;
    }

    double xy = 0.0;
    double xx = 0.0;
    double yy = 0.0;
    for (std::size_t i = start; i < end; ++i) {
        const double xd = static_cast<double>(x[i]);
        const double yd = static_cast<double>(y[i]);
        xy += xd * yd;
        xx += xd * xd;
        yy += yd * yd;
    }

    if (xx <= 1.0e-12 || yy <= 1.0e-12) {
        return 0.0;
    }
    return xy / std::sqrt(xx * yy);
}

double deltaRatio(const std::vector<float>& x,
                  const std::vector<float>& y,
                  std::size_t start,
                  std::size_t end)
{
    if (start >= end || end > x.size() || end > y.size()) {
        return 0.0;
    }

    std::vector<float> diff(end - start, 0.0f);
    for (std::size_t i = start; i < end; ++i) {
        diff[i - start] = y[i] - x[i];
    }

    const double inputLevel = rms(x, start, end);
    if (inputLevel <= 1.0e-12) {
        return rms(diff, 0, diff.size());
    }
    return rms(diff, 0, diff.size()) / inputLevel;
}

SignalMetrics analyzeBurstSignal(const std::vector<float>& input,
                                 const std::vector<float>& output)
{
    SignalMetrics metrics;
    metrics.inputRms = rms(input, 0, kBurstActiveSamples);
    metrics.outputRms = rms(output, 0, kBurstActiveSamples);
    metrics.outputMidbandRms = midbandRms(output, 0, kBurstActiveSamples);
    metrics.deltaRatio = deltaRatio(input, output, 0, kBurstActiveSamples);
    metrics.correlation = correlation(input, output, 0, kBurstActiveSamples);
    metrics.tailRms = rms(output, kBurstActiveSamples, output.size());
    metrics.peakAbs = peakAbs(output, 0, kBurstActiveSamples);
    metrics.hotSampleRatio = ratioAboveAbsThreshold(output, 0, kBurstActiveSamples, 0.85);
    metrics.clipSampleRatio = ratioAboveAbsThreshold(output, 0, kBurstActiveSamples, 0.98);
    return metrics;
}

SignalMetrics analyzeSineSignal(const std::vector<float>& input,
                                const std::vector<float>& output)
{
    SignalMetrics metrics;
    const std::size_t start = static_cast<std::size_t>(kSineAnalysisStart);
    const std::size_t end = output.size();

    metrics.inputRms = rms(input, start, end);
    metrics.outputRms = rms(output, start, end);
    metrics.outputMidbandRms = midbandRms(output, start, end);
    metrics.deltaRatio = deltaRatio(input, output, start, end);
    metrics.correlation = correlation(input, output, start, end);
    metrics.peakAbs = peakAbs(output, start, end);
    metrics.hotSampleRatio = ratioAboveAbsThreshold(output, start, end, 0.85);
    metrics.clipSampleRatio = ratioAboveAbsThreshold(output, start, end, 0.98);

    double sumSS = 0.0;
    double sumCC = 0.0;
    double sumSC = 0.0;
    double sumYS = 0.0;
    double sumYC = 0.0;

    for (std::size_t i = start; i < end; ++i) {
        const double phase =
          (kTwoPi * static_cast<double>(kSineFreq) * static_cast<double>(i)) /
          static_cast<double>(kSampleRate);
        const double s = std::sin(phase);
        const double c = std::cos(phase);
        const double y = static_cast<double>(output[i]);
        sumSS += s * s;
        sumCC += c * c;
        sumSC += s * c;
        sumYS += y * s;
        sumYC += y * c;
    }

    const double det = sumSS * sumCC - sumSC * sumSC;
    if (std::fabs(det) > 1.0e-12) {
        const double a = (sumYS * sumCC - sumYC * sumSC) / det;
        const double b = (sumYC * sumSS - sumYS * sumSC) / det;

        double residualSum = 0.0;
        double fittedSum = 0.0;
        for (std::size_t i = start; i < end; ++i) {
            const double phase =
              (kTwoPi * static_cast<double>(kSineFreq) * static_cast<double>(i)) /
              static_cast<double>(kSampleRate);
            const double fitted = a * std::sin(phase) + b * std::cos(phase);
            const double error = static_cast<double>(output[i]) - fitted;
            residualSum += error * error;
            fittedSum += fitted * fitted;
        }

        const double sampleCount = static_cast<double>(end - start);
        const double residualRms = std::sqrt(residualSum / sampleCount);
        const double fittedRms = std::sqrt(fittedSum / sampleCount);
        metrics.fundamentalGain =
          metrics.inputRms > 1.0e-12 ? fittedRms / metrics.inputRms : 0.0;
        metrics.residualRatio =
          metrics.outputRms > 1.0e-12 ? residualRms / metrics.outputRms : 0.0;
    }

    return metrics;
}

std::vector<float> makeBurstInput(float inputScale)
{
    std::vector<float> signal(kBurstTotalSamples, 0.0f);
    for (int i = 0; i < kBurstActiveSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        const double envelope = 0.78 + 0.22 * std::sin(2.0 * kPi * 1.7 * t);
        const double composite =
          0.18 * std::sin(kTwoPi * 110.0 * t) +
          0.12 * std::sin(kTwoPi * 220.0 * t + 0.3) +
          0.08 * std::sin(kTwoPi * 330.0 * t + 0.6) +
          0.04 * std::sin(kTwoPi * 440.0 * t + 0.9);
        signal[i] = static_cast<float>(inputScale * envelope * composite);
    }
    return signal;
}

std::vector<float> makeSineInput(float inputScale)
{
    std::vector<float> signal(kSineSamples, 0.0f);
    for (int i = 0; i < kSineSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        signal[i] =
          static_cast<float>(inputScale * kSineAmp * std::sin(kTwoPi * kSineFreq * t));
    }
    return signal;
}

struct RunResult
{
    std::vector<float> left;
    std::vector<float> right;
    bool nanOrInfDetected = false;
};

RunResult runSignal(Patch& patch,
                    const std::vector<float>& inputLeft,
                    const std::vector<float>& inputRight)
{
    RunResult result;
    result.left.resize(inputLeft.size(), 0.0f);
    result.right.resize(inputRight.size(), 0.0f);

    std::vector<float> leftBlock(kBlockSize, 0.0f);
    std::vector<float> rightBlock(kBlockSize, 0.0f);

    for (std::size_t offset = 0; offset < inputLeft.size(); offset += kBlockSize) {
        const std::size_t blockSize =
          std::min<std::size_t>(static_cast<std::size_t>(kBlockSize), inputLeft.size() - offset);

        std::copy_n(inputLeft.begin() + static_cast<long>(offset),
                    static_cast<long>(blockSize),
                    leftBlock.begin());
        std::copy_n(inputRight.begin() + static_cast<long>(offset),
                    static_cast<long>(blockSize),
                    rightBlock.begin());

        patch.processAudio(std::span<float>(leftBlock.data(), blockSize),
                           std::span<float>(rightBlock.data(), blockSize));

        if (!allFinite(leftBlock.data(), blockSize) || !allFinite(rightBlock.data(), blockSize)) {
            result.nanOrInfDetected = true;
        }

        std::copy_n(leftBlock.begin(),
                    static_cast<long>(blockSize),
                    result.left.begin() + static_cast<long>(offset));
        std::copy_n(rightBlock.begin(),
                    static_cast<long>(blockSize),
                    result.right.begin() + static_cast<long>(offset));
    }

    return result;
}

std::array<float, 3> getDefaults(Patch& patch)
{
    return {
        patch.getParameterMetadata(0).defaultValue,
        patch.getParameterMetadata(1).defaultValue,
        patch.getParameterMetadata(2).defaultValue,
    };
}

ScenarioMetrics runScenario(const std::array<float, 3>& params,
                            float inputScale,
                            bool bypass,
                            bool holdMode)
{
    Patch* patch = Patch::getInstance();

    std::vector<float> workingBuffer(Patch::kWorkingBufferSize, 0.0f);
    patch->setWorkingBuffer(std::span<float, Patch::kWorkingBufferSize>(
      workingBuffer.data(), Patch::kWorkingBufferSize));
    patch->init();

    for (int idx = 0; idx < 3; ++idx) {
        patch->setParamValue(idx, params[idx]);
    }

    if (holdMode) {
        patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
    }
    if (bypass) {
        patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchPress));
    }

    const std::vector<float> burstInput = makeBurstInput(inputScale);
    const std::vector<float> sineInput = makeSineInput(inputScale);

    RunResult burstResult = runSignal(*patch, burstInput, burstInput);
    RunResult sineResult = runSignal(*patch, sineInput, sineInput);

    ScenarioMetrics metrics;
    metrics.burst = analyzeBurstSignal(burstInput, burstResult.left);
    metrics.sine = analyzeSineSignal(sineInput, sineResult.left);
    metrics.nanOrInfDetected = burstResult.nanOrInfDetected || sineResult.nanOrInfDetected;
    return metrics;
}

double diffRatio(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size() || a.empty()) {
        return 0.0;
    }

    std::vector<float> diff(a.size(), 0.0f);
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff[i] = a[i] - b[i];
    }
    const double ref = rms(a, 0, a.size());
    if (ref <= 1.0e-12) {
        return rms(diff, 0, diff.size());
    }
    return rms(diff, 0, diff.size()) / ref;
}

struct ModeDiff
{
    double burstDiffRatio = 0.0;
    double sineDiffRatio = 0.0;
    bool nanOrInfDetected = false;
};

ModeDiff runModeDiff(const std::array<float, 3>& params, float inputScale)
{
    Patch* patch = Patch::getInstance();

    auto runOutput = [&](bool holdMode, const std::vector<float>& input) {
        std::vector<float> workingBuffer(Patch::kWorkingBufferSize, 0.0f);
        patch->setWorkingBuffer(std::span<float, Patch::kWorkingBufferSize>(
          workingBuffer.data(), Patch::kWorkingBufferSize));
        patch->init();
        for (int idx = 0; idx < 3; ++idx) {
            patch->setParamValue(idx, params[idx]);
        }
        if (holdMode) {
            patch->handleAction(static_cast<int>(endless::ActionId::kLeftFootSwitchHold));
        }
        return runSignal(*patch, input, input);
    };

    const std::vector<float> burstInput = makeBurstInput(inputScale);
    const std::vector<float> sineInput = makeSineInput(inputScale);

    const RunResult defaultBurst = runOutput(false, burstInput);
    const RunResult holdBurst = runOutput(true, burstInput);
    const RunResult defaultSine = runOutput(false, sineInput);
    const RunResult holdSine = runOutput(true, sineInput);

    ModeDiff diff;
    diff.burstDiffRatio = diffRatio(defaultBurst.left, holdBurst.left);
    diff.sineDiffRatio = diffRatio(defaultSine.left, holdSine.left);
    diff.nanOrInfDetected = defaultBurst.nanOrInfDetected || holdBurst.nanOrInfDetected ||
                            defaultSine.nanOrInfDetected || holdSine.nanOrInfDetected;
    return diff;
}

// std::cout's default formatting of a non-finite double ("-nan", "inf", ...)
// is not valid JSON. A NaN/Inf-producing patch (exactly what
// nan_or_inf_detected exists to flag) propagates into these metrics via
// sum += x*x etc., so without this the JSON output itself becomes
// unparseable right when a consumer most needs to read the flag. Python's
// json module accepts the capitalized NaN/Infinity/-Infinity tokens as a
// standard extension, so emit those explicitly for any non-finite value.
std::ostream& printJsonDouble(std::ostream& os, double value)
{
    if (std::isnan(value)) {
        return os << "NaN";
    }
    if (std::isinf(value)) {
        return os << (value > 0.0 ? "Infinity" : "-Infinity");
    }
    return os << value;
}

void printSignalMetrics(const SignalMetrics& metrics)
{
    std::cout << "{"
              << "\"input_rms\":";
    printJsonDouble(std::cout, metrics.inputRms);
    std::cout << ",\"output_rms\":";
    printJsonDouble(std::cout, metrics.outputRms);
    std::cout << ",\"output_midband_rms\":";
    printJsonDouble(std::cout, metrics.outputMidbandRms);
    std::cout << ",\"delta_ratio\":";
    printJsonDouble(std::cout, metrics.deltaRatio);
    std::cout << ",\"correlation\":";
    printJsonDouble(std::cout, metrics.correlation);
    std::cout << ",\"fundamental_gain\":";
    printJsonDouble(std::cout, metrics.fundamentalGain);
    std::cout << ",\"residual_ratio\":";
    printJsonDouble(std::cout, metrics.residualRatio);
    std::cout << ",\"tail_rms\":";
    printJsonDouble(std::cout, metrics.tailRms);
    std::cout << ",\"peak_abs\":";
    printJsonDouble(std::cout, metrics.peakAbs);
    std::cout << ",\"hot_sample_ratio\":";
    printJsonDouble(std::cout, metrics.hotSampleRatio);
    std::cout << ",\"clip_sample_ratio\":";
    printJsonDouble(std::cout, metrics.clipSampleRatio);
    std::cout << "}";
}

void printScenarioMetrics(const ScenarioMetrics& metrics)
{
    std::cout << "{"
              << "\"burst\":";
    printSignalMetrics(metrics.burst);
    std::cout << ",\"sine\":";
    printSignalMetrics(metrics.sine);
    std::cout << ",\"nan_or_inf_detected\":" << (metrics.nanOrInfDetected ? "true" : "false")
              << "}";
}

// Returns true if any point in the sweep detected a NaN/Inf, so callers can
// fold it into the top-level "any_nan_or_inf" summary.
bool printSweep(const std::array<float, 3>& defaults, float inputScale, int paramIdx)
{
    bool anyNonFinite = false;
    std::cout << "[";
    bool first = true;
    for (float value : kSweepValues) {
        std::array<float, 3> params = defaults;
        params[paramIdx] = value;
        const ScenarioMetrics metrics = runScenario(params, inputScale, false, false);
        anyNonFinite = anyNonFinite || metrics.nanOrInfDetected;
        if (!first) {
            std::cout << ",";
        }
        first = false;
        std::cout << "{"
                  << "\"value\":" << value << ","
                  << "\"metrics\":";
        printScenarioMetrics(metrics);
        std::cout << "}";
    }
    std::cout << "]";
    return anyNonFinite;
}
}

int main(int argc, char** argv)
{
    const std::string patchName = argc > 1 ? argv[1] : "unknown";
    const float inputScale = argc > 2 ? std::strtof(argv[2], nullptr) : 1.0f;

    Patch* patch = Patch::getInstance();
    std::vector<float> bootstrapWorkingBuffer(Patch::kWorkingBufferSize, 0.0f);
    patch->setWorkingBuffer(std::span<float, Patch::kWorkingBufferSize>(
      bootstrapWorkingBuffer.data(), Patch::kWorkingBufferSize));
    patch->init();
    const std::array<float, 3> defaults = getDefaults(*patch);

    const ScenarioMetrics active = runScenario(defaults, inputScale, false, false);
    const ScenarioMetrics bypassed = runScenario(defaults, inputScale, true, false);
    const ModeDiff holdMode = runModeDiff(defaults, inputScale);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "{"
              << "\"patch\":\"" << patchName << "\","
              << "\"input_scale\":" << inputScale << ","
              << "\"defaults\":["
              << defaults[0] << ","
              << defaults[1] << ","
              << defaults[2] << "],"
              << "\"active\":";
    printScenarioMetrics(active);
    std::cout << ",\"bypassed\":";
    printScenarioMetrics(bypassed);
    std::cout << ",\"hold_mode_diff\":{"
              << "\"burst_diff_ratio\":";
    printJsonDouble(std::cout, holdMode.burstDiffRatio);
    std::cout << ",\"sine_diff_ratio\":";
    printJsonDouble(std::cout, holdMode.sineDiffRatio);
    std::cout << ",\"nan_or_inf_detected\":" << (holdMode.nanOrInfDetected ? "true" : "false")
              << "},"
              << "\"sweeps\":{"
              << "\"param0\":";
    const bool sweep0NonFinite = printSweep(defaults, inputScale, 0);
    std::cout << ",\"param1\":";
    const bool sweep1NonFinite = printSweep(defaults, inputScale, 1);
    std::cout << ",\"param2\":";
    const bool sweep2NonFinite = printSweep(defaults, inputScale, 2);
    std::cout << "}";

    // One top-level union of every finiteness check this probe ran, so a
    // consumer (scripts/analyze_effects.py) can flag a NaN/Inf-producing
    // patch as an automatic, category-independent failure without having
    // to walk the whole nested structure looking for it.
    const bool anyNonFinite = active.nanOrInfDetected || bypassed.nanOrInfDetected ||
                              holdMode.nanOrInfDetected || sweep0NonFinite ||
                              sweep1NonFinite || sweep2NonFinite;
    std::cout << ",\"any_nan_or_inf\":" << (anyNonFinite ? "true" : "false")
              << "}"
              << "\n";
    return EXIT_SUCCESS;
}
