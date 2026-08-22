// tests/oversample_alias_probe.cpp
//
// Standalone experiment: does 2x-oversampling the WDF diode-pair nonlinearity
// in effects/tube_screamer_wdf.cpp reduce aliasing, and at what CPU cost?
// Measures, does not assume -- see docs/aliasing-oversampling-experiment.md
// for the results and conclusion.
//
// This file is deliberately NOT under effects/ (so tests/check_patches.sh's
// effects/*.cpp glob never picks it up as a patch) and does NOT modify
// effects/tube_screamer_wdf.cpp in any way -- it is 100% additive tooling.
// Whether to apply oversampling to the shipped effect is a separate, later
// decision gated on these numbers.
//
// `#include "../effects/tube_screamer_wdf.cpp"` is unusual (including a
// .cpp, not a header) but deliberate: WdfAntiparallelDiodePair lives in
// that file's anonymous namespace, so it isn't visible from a separately
// compiled translation unit. Including the .cpp directly guarantees this
// experiment tests the *exact* shipped nonlinearity with zero possibility
// of copy/paste drift, at the cost of this file only ever being compiled
// standalone (never linked with anything else -- it defines its own main()
// and never calls the unused Patch::getInstance() the include drags in).
//
// Usage:
//   g++ -std=c++20 -O3 -fno-exceptions -fno-rtti -fsingle-precision-constant -I source
//       tests/oversample_alias_probe.cpp -o build/oversample_probe
//   build/oversample_probe            # writes signal captures + JSON sidecars
//   build/oversample_probe --bench    # prints cycle-ratio ordering signal

#include "../effects/tube_screamer_wdf.cpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define OVERSAMPLE_PROBE_HAVE_RDTSC 1
#endif

namespace
{

// TS808 defaults (effects/tube_screamer_wdf.cpp's getTs808Voice(), the
// three fields this experiment touches) -- a realistic operating point for
// the nonlinearity under test, not a claim about the whole voice.
constexpr float kPortResistance = 0.29f;
constexpr float kDiodeStrength  = 0.14f;
constexpr float kThermal        = 0.14f;
constexpr float kSampleRateHz   = 48000.0f;

float diodeProcess(WdfAntiparallelDiodePair& diode, float x)
{
    return diode.process(x, kPortResistance, kDiodeStrength, kThermal);
}

// --- Signal generators ------------------------------------------------
// Frequencies are bin-exact for N=8192 @ 48 kHz (48000/8192 = 5.859375 Hz
// per bin) and deliberately not simple submultiples of 48000, so aliased
// energy lands in bins distinguishable from the fundamental/harmonics.

constexpr int kBlockLen = 8192;

std::vector<float> genSweepTone(float amplitude)
{
    constexpr int   kBin  = 1021;              // ~5982.66 Hz
    const float     freq  = kBin * kSampleRateHz / kBlockLen;
    const float     w     = 6.283185307f * freq / kSampleRateHz;
    std::vector<float> out(kBlockLen);
    for (int i = 0; i < kBlockLen; ++i)
        out[i] = amplitude * sinf(w * static_cast<float>(i));
    return out;
}

std::vector<float> genTwoTone(float amplitude)
{
    constexpr int kBin1 = 853;                 // ~4998.05 Hz
    constexpr int kBin2 = 1041;                // ~6099.61 Hz
    const float   f1 = kBin1 * kSampleRateHz / kBlockLen;
    const float   f2 = kBin2 * kSampleRateHz / kBlockLen;
    const float   w1 = 6.283185307f * f1 / kSampleRateHz;
    const float   w2 = 6.283185307f * f2 / kSampleRateHz;
    std::vector<float> out(kBlockLen);
    for (int i = 0; i < kBlockLen; ++i)
        out[i] = 0.5f * amplitude * (sinf(w1 * static_cast<float>(i)) + sinf(w2 * static_cast<float>(i)));
    return out;
}

// --- Processing modes ---------------------------------------------------

std::vector<float> process1x(const std::vector<float>& in)
{
    WdfAntiparallelDiodePair diode;
    diode.reset();
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
        out[i] = diodeProcess(diode, in[i]);
    return out;
}

// Naive 2x: linear-interpolated midpoint upsample, process at both points,
// boxcar-average decimate. Cheapest possible oversampling; only -3 dB at
// Nyquist/2 as a decimation filter (see docs/aliasing-oversampling-experiment.md).
std::vector<float> process2xNaive(const std::vector<float>& in)
{
    WdfAntiparallelDiodePair diode;
    diode.reset();
    std::vector<float> out(in.size());
    float prev = 0.0f;
    for (size_t i = 0; i < in.size(); ++i)
    {
        const float mid   = 0.5f * (prev + in[i]);
        const float yMid  = diodeProcess(diode, mid);
        const float yFull = diodeProcess(diode, in[i]);
        out[i] = 0.5f * (yMid + yFull);
        prev = in[i];
    }
    return out;
}

// 7-tap multiplierless halfband FIR: h = [-1, 0, 9, 16, 9, 0, -1] / 32.
// DC gain 1.0, exactly -6.02 dB at Nyquist/2 (the textbook halfband
// property), ~-19 dB by 0.35*fs, ~-55 dB by 0.45*fs -- roughly 15-20 dB
// more stopband attenuation than the naive boxcar in the critical
// near-Nyquist region, for four real multiply-adds (the zero taps are free).
constexpr int   kHalfbandTaps = 7;
constexpr float kHalfbandCoeffs[kHalfbandTaps] = {
    -1.0f / 32.0f, 0.0f, 9.0f / 32.0f, 16.0f / 32.0f, 9.0f / 32.0f, 0.0f, -1.0f / 32.0f
};

std::vector<float> halfbandFilter(const std::vector<float>& in)
{
    constexpr int kCenter = kHalfbandTaps / 2;
    std::vector<float> out(in.size(), 0.0f);
    for (size_t i = 0; i < in.size(); ++i)
    {
        float acc = 0.0f;
        for (int k = 0; k < kHalfbandTaps; ++k)
        {
            const long idx = static_cast<long>(i) + k - kCenter;
            if (idx >= 0 && idx < static_cast<long>(in.size()))
                acc += kHalfbandCoeffs[k] * in[static_cast<size_t>(idx)];
        }
        out[i] = acc;
    }
    return out;
}

// Textbook 2x oversampling: zero-stuff upsample (gain-compensated by the
// upsample factor) -> halfband lowpass (removes the zero-stuffing image) ->
// process at 2x rate -> halfband lowpass again (anti-aliases before the
// drop) -> decimate by keeping every other sample.
std::vector<float> process2xHalfband(const std::vector<float>& in)
{
    std::vector<float> upsampled(in.size() * 2, 0.0f);
    for (size_t i = 0; i < in.size(); ++i)
        upsampled[2 * i] = in[i] * 2.0f;
    const std::vector<float> interpolated = halfbandFilter(upsampled);

    WdfAntiparallelDiodePair diode;
    diode.reset();
    std::vector<float> processed(interpolated.size());
    for (size_t i = 0; i < interpolated.size(); ++i)
        processed[i] = diodeProcess(diode, interpolated[i]);

    const std::vector<float> decimationFiltered = halfbandFilter(processed);
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
        out[i] = decimationFiltered[2 * i];
    return out;
}

// --- Output ---------------------------------------------------------------

bool writeCapture(const std::string& outDir, const std::string& name,
                  const std::vector<float>& samples, float amplitude,
                  const std::string& signalKind)
{
    const std::string binPath  = outDir + "/" + name + ".f32";
    const std::string jsonPath = outDir + "/" + name + ".json";

    FILE* bin = fopen(binPath.c_str(), "wb");
    if (!bin) { fprintf(stderr, "cannot open %s\n", binPath.c_str()); return false; }
    fwrite(samples.data(), sizeof(float), samples.size(), bin);
    fclose(bin);

    FILE* js = fopen(jsonPath.c_str(), "w");
    if (!js) { fprintf(stderr, "cannot open %s\n", jsonPath.c_str()); return false; }
    fprintf(js,
            "{\"name\":\"%s\",\"sample_rate\":%.1f,\"n\":%zu,\"amplitude\":%.6f,"
            "\"signal_kind\":\"%s\"}\n",
            name.c_str(), (double) kSampleRateHz, samples.size(), (double) amplitude,
            signalKind.c_str());
    fclose(js);
    return true;
}

void runCaptures(const std::string& outDir)
{
    std::filesystem::create_directories(outDir);

    struct Mode { const char* name; std::vector<float> (*fn)(const std::vector<float>&); };
    const Mode modes[] = {
        {"1x", process1x},
        {"2x_naive", process2xNaive},
        {"2x_halfband", process2xHalfband},
    };

    struct Amp { const char* name; float value; };
    const Amp amps[] = {
        {"mild", 0.3f},
        {"moderate", 0.9f},
        {"hard", 2.4f},
    };

    int written = 0;
    for (const auto& amp : amps)
    {
        const std::vector<float> sweep   = genSweepTone(amp.value);
        const std::vector<float> twoTone = genTwoTone(amp.value);
        for (const auto& mode : modes)
        {
            {
                const std::string name = std::string(mode.name) + "_sweep_" + amp.name;
                const std::vector<float> out = mode.fn(sweep);
                if (writeCapture(outDir, name, out, amp.value, "sweep")) ++written;
            }
            {
                const std::string name = std::string(mode.name) + "_twotone_" + amp.name;
                const std::vector<float> out = mode.fn(twoTone);
                if (writeCapture(outDir, name, out, amp.value, "twotone")) ++written;
            }
        }
    }
    printf("wrote %d capture pairs (.f32 + .json) to %s\n", written, outDir.c_str());
}

#ifdef OVERSAMPLE_PROBE_HAVE_RDTSC
double measureCyclesPerSample(std::vector<float> (*fn)(const std::vector<float>&))
{
    constexpr int kWarmupIters    = 50;
    constexpr int kTrials         = 20;
    const std::vector<float> input = genSweepTone(0.9f);

    volatile float sink = 0.0f;
    for (int i = 0; i < kWarmupIters; ++i)
    {
        const std::vector<float> out = fn(input);
        sink += out[0];
    }

    uint64_t best = UINT64_MAX;
    for (int trial = 0; trial < kTrials; ++trial)
    {
        _mm_lfence();
        const uint64_t t0 = __rdtsc();
        const std::vector<float> out = fn(input);
        _mm_lfence();
        const uint64_t t1 = __rdtsc();
        sink += out[0];
        if (t1 - t0 < best) best = t1 - t0;
    }
    (void) sink;
    return static_cast<double>(best) / static_cast<double>(input.size());
}

void runBench()
{
    const double c1x       = measureCyclesPerSample(process1x);
    const double c2xNaive  = measureCyclesPerSample(process2xNaive);
    const double c2xHalf   = measureCyclesPerSample(process2xHalfband);

    printf("host cycles/sample (min-of-20, x86 -- NOT Cortex-M7 numbers, "
          "ordering signal only):\n");
    printf("  1x:          %.2f\n", c1x);
    printf("  2x_naive:    %.2f  (ratio %.3fx)\n", c2xNaive, c2xNaive / c1x);
    printf("  2x_halfband: %.2f  (ratio %.3fx)\n", c2xHalf, c2xHalf / c1x);
}
#endif

}  // namespace

int main(int argc, char** argv)
{
    bool bench = false;
    std::string outDir = "build/oversample_experiment";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--bench") == 0) bench = true;
        else outDir = argv[i];
    }

    if (bench)
    {
#ifdef OVERSAMPLE_PROBE_HAVE_RDTSC
        runBench();
#else
        fprintf(stderr, "--bench requires __rdtsc (x86 host); not available on this build.\n");
        return 1;
#endif
    }
    else
    {
        runCaptures(outDir);
    }
    return 0;
}
