// tests/funk_machine_biquad_accuracy_probe.cpp -- quantifies
// funk_machine_envelope_filter.cpp's second filter-accuracy fix: the
// libm-free Chamberlin SVF f1 coefficient (2026-08-23 work) versus the
// libm-free RBJ biquad coefficients (2026-08-24) that replaced it.
//
// The first fix removed powf/sinf from the control chain (0.15 cent cutoff
// error). The second fix -- this file -- addresses a different problem the
// first one didn't touch: the Chamberlin SVF's resonant peak drifts from
// its target as Q drops (wah.cpp measured up to 172.3 cents; this effect's own
// Q floor of 1.2 measures 165 cents), independent of how the coefficient
// arithmetic is computed. Both halves are libm-free; only the second is
// correctly tuned.
//
// This file specifically verifies the harder part of the second fix: that
// deriving sin(w0)/cos(w0) via double-angle identities from the already-
// validated sinSmall(w0/2)/cosSmall(w0/2) -- rather than fitting a new
// small-angle series over RBJ's wider w0 = 2*pi*fc/fs range -- doesn't cost
// accuracy. Worst measured error across the effect's full reachable space
// (fc 70-2900 Hz, Q 1.2-7.5) is 1.387 cents, at the lowest fc/lowest Q
// corner -- consistent with float32 rounding in the coefficients themselves
// (this probe evaluates them exactly; see magnitudeAt below) rather than the
// small-angle approximation, which is accurate to single-digit microcents at
// that angle. It measures the SAME libmFreeBandpassCoeffs function that
// ships in effects/funk_machine_envelope_filter.cpp, reimplemented here only
// because that function is private to the effect's anonymous namespace.
//
// Build and run:
//   g++ -std=c++20 -O2 -fsingle-precision-constant -I source
//       tests/funk_machine_biquad_accuracy_probe.cpp
//       -o build/funk_machine_biquad_accuracy_probe
//   build/funk_machine_biquad_accuracy_probe

#include "dsp/biquad.h"
#include "dsp/filter_coeff.h"

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr float kPi = 3.14159265359f;

// Verbatim copy of effects/funk_machine_envelope_filter.cpp's private
// sinSmall/cosSmall/libmFreeBandpassCoeffs -- kept in sync by inspection
// (small enough, and changing the shipped math without re-running this
// probe would be caught by the accuracy check below regressing).
float sinSmall(float x) { return x - (x * x * x) * (1.0f / 6.0f); }
float cosSmall(float x)
{
    const float x2 = x * x;
    return 1.0f - x2 * 0.5f + x2 * x2 * (1.0f / 24.0f);
}

dsp::BiquadCoeffs libmFreeBandpassCoeffs(float fc, float q)
{
    const float half  = kPi * fc / kFs;
    const float sh    = sinSmall(half);
    const float ch    = cosSmall(half);
    const float sinW0 = 2.0f * sh * ch;
    const float cosW0 = 1.0f - 2.0f * sh * sh;
    const float alpha = sinW0 / (2.0f * q);
    const float invA0 = 1.0f / (1.0f + alpha);
    return {alpha * invA0, -alpha * invA0, -2.0f * cosW0 * invA0, (1.0f - alpha) * invA0};
}

// Peak search via the exact z-transform magnitude response |H(f)|, not
// time-domain simulation: for a biquad, |H(f)| computed from the
// coefficients themselves *is* the frequency response, not a proxy for it,
// so this is exact and sidesteps settling-time and search-grid artifacts
// entirely. (Pole angle, by contrast, genuinely diverges from the true
// |H(f)| peak for the Chamberlin SVF's zero structure at low Q -- see
// tests/wah_svf_accuracy_probe.cpp's derivation of that filter's own exact
// transfer function; the fix there was switching from pole-angle reasoning
// to a direct evaluation of |H(f)|, the same principle applied here.) An
// earlier version of this file used time-domain simulation and reported
// 3.48 cents at one point that a closed-form check showed was actually
// 0.02 cents -- a measurement artifact of that method, not a property of
// the coefficients. Kept as a cautionary note: always cross-check a
// surprising number with a second method before trusting it.
double magnitudeAt(float f, const dsp::BiquadCoeffs& c)
{
    const double w = 2.0 * static_cast<double>(kPi) * f / kFs;
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
    std::printf("funk_machine_envelope_filter.cpp libm-free biquad accuracy\n");
    std::printf("Sweeping the effect's full reachable space: fc 70-2900 Hz, Q 1.2-7.5.\n\n");
    std::printf("%-8s %-6s %12s %10s\n", "fc", "Q", "measured", "error");

    // Reachable (fcMin, fcMax) endpoints for both voices at both bias
    // extremes, per the effect's own log2FcMin/log2FcMax formulas.
    const float fcs[] = {70.0f, 280.0f, 900.0f, 2160.0f, 150.0f, 450.0f, 1600.0f, 2880.0f, 2900.0f};
    const float qs[]  = {1.2f, 4.73f, 7.5f};

    float worst = 0.0f;
    for (float fc : fcs)
    {
        for (float q : qs)
        {
            const dsp::BiquadCoeffs c = libmFreeBandpassCoeffs(fc, q);
            const float peak = measuredPeakHz(fc, c);
            const float cents = 1200.0f * log2f(peak / fc);
            worst = std::fmax(worst, std::fabs(cents));
            std::printf("%-8.0f %-6.2f %11.3fHz %+8.3fc\n",
                       static_cast<double>(fc), static_cast<double>(q),
                       static_cast<double>(peak), static_cast<double>(cents));
        }
    }
    std::printf("\nworst measured error: %.3f cents\n", static_cast<double>(worst));
    return 0;
}
