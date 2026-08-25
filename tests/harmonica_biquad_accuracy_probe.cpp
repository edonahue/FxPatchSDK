// tests/harmonica_biquad_accuracy_probe.cpp -- quantifies
// effects/harmonica.cpp's filter-accuracy fix: the Chamberlin SVF formant
// filters (form1 swept, form2 fixed) versus the RBJ constant-peak-gain
// bandpass biquad that replaced them.
//
// harmonica.cpp's Q floor (2.0-3.0, both voicings) is the mildest of the
// three effects fixed this session -- wah.cpp's is 1.0,
// funk_machine_envelope_filter.cpp's is 1.2 -- so its detuning was the
// smallest of the three: 51 cents worst case, versus 172.3 and 165. Still
// measured, not assumed, and still fixed for the same reason.
//
// Method: exact z-transform magnitude response |H(f)|, same as
// tests/funk_machine_biquad_accuracy_probe.cpp and for the same reason --
// for a biquad, |H(f)| computed from the coefficients *is* the frequency
// response, not a proxy for it, so this is exact and sidesteps
// settling-time/search-grid artifacts a time-domain peak search would carry.
//
// Build and run:
//   g++ -std=c++20 -O2 -fsingle-precision-constant -I source
//       tests/harmonica_biquad_accuracy_probe.cpp
//       -o build/harmonica_biquad_accuracy_probe
//   build/harmonica_biquad_accuracy_probe

#include "dsp/biquad.h"
#include "dsp/filter_coeff.h"

#include <cmath>
#include <complex>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;

double magnitudeAt(float f, const dsp::BiquadCoeffs& c)
{
    const double w = 2.0 * kPi * f / kFs;
    const std::complex<double> zInv = std::polar(static_cast<double>(1.0), -w);
    const std::complex<double> b0{static_cast<double>(c.b0), 0.0};
    const std::complex<double> b2{static_cast<double>(c.b2), 0.0};
    const std::complex<double> a1{static_cast<double>(c.a1), 0.0};
    const std::complex<double> a2{static_cast<double>(c.a2), 0.0};
    const std::complex<double> num = b0 + b2 * zInv * zInv;
    const std::complex<double> den = std::complex<double>{1.0, 0.0} + a1 * zInv + a2 * zInv * zInv;
    return std::abs(num / den);
}

float measuredPeakHz(float fc, const dsp::BiquadCoeffs& coeffs)
{
    float bestFreq = fc;
    double bestMag = -1.0;
    for (float testFc = fc * 0.9f; testFc <= fc * 1.1f; testFc += fc * 0.00001f)
    {
        const double mag = magnitudeAt(testFc, coeffs);
        if (mag > bestMag) { bestMag = mag; bestFreq = testFc; }
    }
    return bestFreq;
}

} // namespace

int main()
{
    std::printf("harmonica.cpp resonant-peak accuracy: RBJ biquad\n");
    std::printf("Sweeping the effect's full reachable space:\n");
    std::printf("  form1 (swept): fc 320-2500 Hz, Q in {3.0 Open, 6.0 Cupped}\n");
    std::printf("  form2 (fixed): fc 1700 Hz, Q 2.0\n\n");
    std::printf("%-30s %-8s %-6s %12s %10s\n", "point", "fc", "Q", "measured", "error");

    float worst = 0.0f;
    const struct { const char* label; float fc, q; } points[] = {
        {"form1 heel, Open (Q3.0)",   320.0f, 3.0f},
        {"form1 toe,  Open (Q3.0)",  2500.0f, 3.0f},
        {"form1 heel, Cupped (Q6.0)", 320.0f, 6.0f},
        {"form1 toe,  Cupped (Q6.0)", 2000.0f, 6.0f},
        {"form1 mid,  Open (Q3.0)",   896.0f, 3.0f},  // ~sqrt(320*2500)
        {"form2 fixed (Q2.0)",       1700.0f, 2.0f},
    };
    for (const auto& p : points)
    {
        const dsp::BiquadCoeffs c = dsp::rbjBandpassCoeffs(p.fc, p.q, kFs);
        const float peak = measuredPeakHz(p.fc, c);
        const float cents = 1200.0f * log2f(peak / p.fc);
        worst = std::fmax(worst, std::fabs(cents));
        std::printf("%-30s %-8.0f %-6.1f %11.3fHz %+8.3fc\n", p.label,
                   static_cast<double>(p.fc), static_cast<double>(p.q),
                   static_cast<double>(peak), static_cast<double>(cents));
    }
    std::printf("\nworst measured error: %.3f cents\n", static_cast<double>(worst));
    std::printf("(dsp::rbjBandpassCoeffs is the same function used by wah.cpp and\n");
    std::printf(" funk_machine_envelope_filter.cpp -- see tests/dsp/biquad_test.cpp\n");
    std::printf(" for its corpus-wide accuracy grid, 0.022 cents worst case there.)\n");
    return 0;
}
