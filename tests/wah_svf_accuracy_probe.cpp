// tests/wah_svf_accuracy_probe.cpp -- quantifies the resonant-frequency fix
// applied to effects/wah.cpp: the old Chamberlin SVF (f1 = 2*sin(pi*fc/fs))
// versus the new RBJ constant-peak-gain bandpass biquad
// (dsp::rbjBandpassCoeffs / dsp::BandpassBiquad), across the exact fc/Q
// space wah.cpp's own knobs can reach.
//
// Method: exact z-transform magnitude response |H(f)|, not time-domain
// simulation, for BOTH filters -- same reasoning as
// tests/funk_machine_biquad_accuracy_probe.cpp and
// tests/harmonica_biquad_accuracy_probe.cpp: for a rational transfer
// function, |H(f)| computed directly from its coefficients *is* the
// frequency response, not a proxy for it, so it sidesteps settling-time and
// search-grid artifacts entirely.
//
// This file originally used a coarse time-domain grid search (fc*0.002 step,
// about 3.46 cents/step) and reported a worst-case old-filter error of
// 174.4 cents. That number was itself a grid artifact: at fc=2500, Q=1, the
// search's grid points bracketing the true peak were 2760 Hz and 2765 Hz,
// and the coarser step picked the wrong side. The exact closed-form method
// below (derived by converting the Chamberlin recursion's state-space form
// to a transfer function, the same way any other IIR structure's H(z) is
// found) puts the true worst case at 172.3 cents -- still a large, real
// detuning, just not the number the coarse grid reported. A finer-grid,
// longer-settle time-domain re-check independently converges to the same
// 172.3-172.5 cents, confirming the closed form (not the original
// coarse-grid result) is correct. Kept as a cautionary note, matching
// funk_machine_biquad_accuracy_probe.cpp's: always cross-check a surprising
// number with a second method before trusting it -- including numbers this
// repo already shipped.
//
// The old Chamberlin recursion has no repo primitive (svfF1 remains, but the
// flawed *recursion* it fed was only ever inlined at each call site, and
// wah.cpp's own copy is gone after the fix), so its transfer function is
// derived here from the recursion directly:
//
//   low[n]  = low[n-1] + f1*band[n-1]
//   hi[n]   = in[n] - low[n-1] - (f1+q1)*band[n-1]
//   band[n] = -f1*low[n-1] + (1 - f1^2 - f1*q1)*band[n-1] + f1*in[n]
//
// with output y[n] = band[n]. Writing this as a state-space system
// x[n] = A*x[n-1] + B*in[n], y[n] = C*x[n] with x = [low, band]^T,
// A = [[1, f1], [-f1, d]] (d = 1 - f1^2 - f1*q1), B = [0, f1]^T, C = [0, 1],
// the standard H(z) = C*(zI-A)^-1*B conversion gives:
//
//   H(z) = f1*(z-1) / (z^2 - (d+1)*z + (d+f1^2))
//
// Build and run:
//   g++ -std=c++20 -O2 -fsingle-precision-constant -I source
//       tests/wah_svf_accuracy_probe.cpp -o build/wah_svf_accuracy_probe
//   build/wah_svf_accuracy_probe

#include "dsp/biquad.h"
#include "dsp/filter_coeff.h"

#include <cmath>
#include <complex>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;

double oldMagnitudeAt(float f, float fc, float q)
{
    const double f1 = 2.0 * std::sin(kPi * static_cast<double>(fc) / kFs);
    const double q1 = 1.0 / static_cast<double>(q);
    const double d = 1.0 - f1 * f1 - f1 * q1;
    const double w = 2.0 * kPi * f / kFs;
    const std::complex<double> z = std::polar(static_cast<double>(1.0), w);
    const std::complex<double> num = f1 * (z - std::complex<double>{1.0, 0.0});
    const std::complex<double> den = z * z - (d + 1.0) * z + (d + f1 * f1);
    return std::abs(num / den);
}

double newMagnitudeAt(float f, const dsp::BiquadCoeffs& c)
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

float oldPeakHz(float fc, float q)
{
    float bestFreq = fc;
    double bestMag = -1.0;
    for (float testFc = fc * 0.5f; testFc <= fc * 1.6f; testFc += fc * 0.00001f)
    {
        const double mag = oldMagnitudeAt(testFc, fc, q);
        if (mag > bestMag) { bestMag = mag; bestFreq = testFc; }
    }
    return bestFreq;
}

float newPeakHz(float fc, float q)
{
    const dsp::BiquadCoeffs c = dsp::rbjBandpassCoeffs(fc, q, kFs);
    float bestFreq = fc;
    double bestMag = -1.0;
    for (float testFc = fc * 0.5f; testFc <= fc * 1.6f; testFc += fc * 0.00001f)
    {
        const double mag = newMagnitudeAt(testFc, c);
        if (mag > bestMag) { bestMag = mag; bestFreq = testFc; }
    }
    return bestFreq;
}

} // namespace

int main()
{
    std::printf("wah.cpp resonant-peak accuracy: old Chamberlin SVF vs new RBJ biquad\n");
    std::printf("Sweeping wah.cpp's exact reachable space: fc 350-2500 Hz, Q 1-10.\n\n");
    std::printf("%-8s %-6s %12s %10s %12s %10s\n",
               "fc", "Q", "old_peak", "old_err", "new_peak", "new_err");

    float worstOldCents = 0.0f, worstNewCents = 0.0f;
    const float fcs[] = {350.0f, 935.0f, 1425.0f, 2200.0f, 2500.0f};
    const float qs[]  = {1.0f, 4.5f, 7.0f, 10.0f};
    for (float fc : fcs)
    {
        for (float q : qs)
        {
            const float oldPeak = oldPeakHz(fc, q);
            const float newPeak = newPeakHz(fc, q);
            const float oldCents = 1200.0f * log2f(oldPeak / fc);
            const float newCents = 1200.0f * log2f(newPeak / fc);
            worstOldCents = std::fmax(worstOldCents, std::fabs(oldCents));
            worstNewCents = std::fmax(worstNewCents, std::fabs(newCents));
            std::printf("%-8.0f %-6.1f %11.1fHz %+8.1fc %11.1fHz %+8.3fc\n",
                       static_cast<double>(fc), static_cast<double>(q),
                       static_cast<double>(oldPeak), static_cast<double>(oldCents),
                       static_cast<double>(newPeak), static_cast<double>(newCents));
        }
    }

    std::printf("\nworst old error: %.1f cents\n", static_cast<double>(worstOldCents));
    std::printf("worst new error: %.3f cents\n", static_cast<double>(worstNewCents));
    return 0;
}
