// tests/funk_machine_approx_probe.cpp — accuracy probe for replacing
// funk_machine_envelope_filter.cpp's powf+sinf control-rate chain with
// libm-free approximations.
//
// docs/endl-corpus-study.md found that no Polyend or Playground patch links a
// newlib transcendental, while ours are built on them. funk_machine runs
// powf() and sinf() every 8 samples -- a 6 kHz control rate, so 12,000
// transcendental calls per second -- to turn the detector envelope into a
// Chamberlin SVF coefficient.
//
// The exact chain is:
//     fcMin = a * powf(b, bias)          (per block)
//     fcMax = c * powf(d, bias)          (per block)
//     fc    = fcMin * powf(fcMax/fcMin, e)
//     f1    = 2 * sinf(pi * fc / fs)
//
// Two observations collapse it:
//
//   1. b, d are compile-time constants, so powf(b, bias) == exp2(bias*log2(b))
//      with log2(b) a constant. The whole frequency map can be carried in the
//      log2 domain, which makes fcRatio's logarithm an affine function of bias
//      -- no runtime logarithm anywhere.
//   2. fc never exceeds ~2.9 kHz, so pi*fc/fs stays under 0.19 rad, where
//      sin(x) = x - x^3/6 is accurate to better than 1e-5 relative.
//
// This probe sweeps the entire reachable parameter space and reports the error
// in Hz, relative terms, and cents. Cents is the metric that matters: a filter
// cutoff error under a cent or so is inaudible.
//
// Build and run:
//   g++ -std=c++20 -O2 -fsingle-precision-constant -Wall -Wextra
//       tests/funk_machine_approx_probe.cpp -o build/funk_approx
//   build/funk_approx

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr float kPi = 3.14159265359f;

// ---------------------------------------------------------------- exact path

struct Window { float fcMin, fcMax; };

Window exactWindow(bool bass, float bias)
{
    if (bass) {
        return {70.0f * powf(4.0f, bias), 900.0f * powf(2.4f, bias)};
    }
    return {150.0f * powf(3.0f, bias), 1600.0f * powf(1.8f, bias)};
}

float exactF1(bool bass, float bias, float exponent)
{
    const Window w = exactWindow(bass, bias);
    const float fc = w.fcMin * powf(w.fcMax / w.fcMin, exponent);
    return 2.0f * sinf(kPi * fc / kFs);
}

// ----------------------------------------------------------- approximate path

// 2^x with no libm call. The integer part becomes an exponent-field write; the
// fractional part uses the truncated series for 2^f = e^(f ln2), which over
// [0,1) is accurate to a few 1e-5 relative at this degree.
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

    // 2^xi by writing the IEEE-754 exponent field directly. The caller's range
    // is bounded well inside float, but clamp anyway so a stray value cannot
    // produce a denormal or an infinity.
    int e = static_cast<int>(xi);
    if (e < -60) { e = -60; }
    if (e > 60)  { e = 60; }
    const uint32_t bits = static_cast<uint32_t>(e + 127) << 23;
    return p * __builtin_bit_cast(float, bits);
}

// sin(x) for small x: two terms of the Taylor series. Over the reachable range
// (x <= 0.19 rad) the first omitted term is x^5/120 < 2.1e-6.
inline float sinSmall(float x)
{
    return x - (x * x * x) * (1.0f / 6.0f);
}

// log2 of each voice's window endpoints, as an affine function of bias. Every
// coefficient here is a compile-time constant derived from the literals in the
// effect, which is what removes the runtime logarithm.
struct LogWindow { float log2Min, log2Max; };

constexpr float kLog2_70   = 6.1292830f;
constexpr float kLog2_900  = 9.8137811f;
constexpr float kLog2_2p4  = 1.2630344f;
constexpr float kLog2_150  = 7.2288187f;
constexpr float kLog2_1600 = 10.6438562f;
constexpr float kLog2_3    = 1.5849625f;
constexpr float kLog2_1p8  = 0.8479969f;

LogWindow approxWindow(bool bass, float bias)
{
    if (bass) {
        // 70 * 4^bias  -> log2 = log2(70) + 2*bias   (4 == 2^2)
        return {kLog2_70 + 2.0f * bias, kLog2_900 + kLog2_2p4 * bias};
    }
    return {kLog2_150 + kLog2_3 * bias, kLog2_1600 + kLog2_1p8 * bias};
}

float approxF1(bool bass, float bias, float exponent)
{
    const LogWindow w = approxWindow(bass, bias);
    const float log2fc = w.log2Min + exponent * (w.log2Max - w.log2Min);
    const float fc = fastExp2(log2fc);
    return 2.0f * sinSmall(kPi * fc / kFs);
}

// ------------------------------------------------------------------ reporting

float f1ToHz(float f1)
{
    // invert f1 = 2 sin(pi fc / fs)
    return asinf(f1 * 0.5f) * kFs / kPi;
}

} // namespace

int main()
{
    double worstRel = 0.0, worstCents = 0.0, worstHz = 0.0;
    float atBias = 0, atExp = 0;
    bool atBass = false;
    int samples = 0;

    for (int v = 0; v < 2; ++v) {
        const bool bass = (v == 0);
        for (int b = 0; b <= 200; ++b) {
            const float bias = static_cast<float>(b) / 200.0f;
            for (int e = 0; e <= 200; ++e) {
                const float exponent = static_cast<float>(e) / 200.0f;
                const float fe = exactF1(bass, bias, exponent);
                const float fa = approxF1(bass, bias, exponent);
                ++samples;

                const double rel = std::fabs(static_cast<double>(fa - fe) / fe);
                const double hzE = f1ToHz(fe), hzA = f1ToHz(fa);
                const double cents = 1200.0 * std::log2(hzA / hzE);
                if (rel > worstRel) {
                    worstRel = rel; worstHz = std::fabs(hzA - hzE);
                    worstCents = std::fabs(cents);
                    atBias = bias; atExp = exponent; atBass = bass;
                }
            }
        }
    }

    std::printf("funk_machine control-chain approximation probe\n");
    std::printf("  samples swept              : %d (2 voices x 201 bias x 201 exponent)\n", samples);
    std::printf("  worst relative error in f1 : %.3e\n", worstRel);
    std::printf("  worst cutoff error         : %.4f Hz\n", worstHz);
    std::printf("  worst cutoff error         : %.5f cents\n", worstCents);
    std::printf("  worst case at              : voice=%s bias=%.3f exponent=%.3f\n",
                atBass ? "Bass" : "GuitarKeys", static_cast<double>(atBias),
                static_cast<double>(atExp));
    std::printf("\n  reference: 1 cent is about the limit of pitch discrimination;\n");
    std::printf("             filter cutoff is far less sensitive than pitch.\n");
    return 0;
}
