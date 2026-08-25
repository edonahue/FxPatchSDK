// tests/dsp/biquad_test.cpp — unit tests for dsp::BandpassBiquad /
// dsp::rbjBandpassCoeffs.
//
// What this covers: the actual property the Chamberlin-SVF-to-biquad swap
// was for -- measured resonant-peak accuracy across an fc x Q grid spanning
// every current caller's real range (wah.cpp: fc 350-2500 Hz, Q 1-10;
// funk_machine_envelope_filter.cpp: fc up to ~2900 Hz, Q down to 1.2;
// harmonica.cpp: Q down to 2.0) -- plus finiteness, reset/reproducibility,
// and DC/Nyquist boundary behavior. Run from the repo root via
// tests/check_dsp.sh.
//
// Method: exact z-transform magnitude response |H(f)|, not time-domain
// simulation. For a biquad, |H(f)| computed from the coefficients themselves
// *is* the frequency response, not a proxy for it, so this is exact and
// sidesteps settling-time/search-grid artifacts a time-domain peak search
// would carry -- see tests/funk_machine_biquad_accuracy_probe.cpp and
// tests/wah_svf_accuracy_probe.cpp for the same method and the reasoning
// behind it (including a prior version of this file's own coarse-grid
// time-domain search, which could not actually support the sub-cent numbers
// it printed).

#include "dsp/biquad.h"
#include "dsp/filter_coeff.h"

#include <cmath>
#include <complex>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;

int failed = 0;
void check(bool cond, const char* msg)
{
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

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

// Exact resonant-peak frequency: scan |H(f)| on a fine grid and return the
// frequency of maximum magnitude.
float measuredPeakHz(float fc, float q)
{
    const dsp::BiquadCoeffs coeffs = dsp::rbjBandpassCoeffs(fc, q, kFs);
    float bestFreq = fc;
    double bestMag = -1.0;
    for (float testFc = fc * 0.6f; testFc <= fc * 1.5f; testFc += fc * 0.00001f)
    {
        const double mag = magnitudeAt(testFc, coeffs);
        if (mag > bestMag) { bestMag = mag; bestFreq = testFc; }
    }
    return bestFreq;
}

} // namespace

int main()
{
    // Resonant-peak accuracy across every current caller's real range.
    // This is the property the whole swap exists for: the Chamberlin SVF
    // measured up to 172.3 cents of error over the same grid (see
    // tests/wah_svf_accuracy_probe.cpp).
    const struct { float fc, q; } grid[] = {
        {350.0f, 1.0f}, {935.0f, 1.0f}, {1425.0f, 1.0f}, {2200.0f, 1.0f}, {2500.0f, 1.0f},
        {350.0f, 5.0f}, {935.0f, 5.0f}, {1425.0f, 5.0f}, {2200.0f, 5.0f}, {2500.0f, 5.0f},
        {350.0f, 10.0f}, {2500.0f, 10.0f},
        {2900.0f, 1.2f},  // funk_machine's floor
        {1700.0f, 2.0f},  // harmonica form2
    };
    float worstCents = 0.0f;
    for (const auto& g : grid)
    {
        const float peak = measuredPeakHz(g.fc, g.q);
        const float cents = 1200.0f * log2f(peak / g.fc);
        check(std::fabs(cents) < 5.0f, "resonant peak should land within 5 cents of target");
        worstCents = std::fmax(worstCents, std::fabs(cents));
    }
    std::fprintf(stderr, "  worst measured peak error across grid: %.3f cents\n",
                static_cast<double>(worstCents));

    // Finiteness and boundedness over a full parameter sweep, including the
    // corner the Chamberlin SVF's own walkthrough flagged as marginal
    // (high fc, high Q) -- confirms the biquad form has no analogous
    // stability cliff.
    for (int i = 0; i <= 50; ++i)
    {
        const float fc = 100.0f + (3000.0f - 100.0f) * (static_cast<float>(i) / 50.0f);
        for (float q : {0.5f, 1.0f, 5.0f, 10.0f, 20.0f})
        {
            const dsp::BiquadCoeffs c = dsp::rbjBandpassCoeffs(fc, q, kFs);
            check(std::isfinite(c.b0) && std::isfinite(c.b2) &&
                  std::isfinite(c.a1) && std::isfinite(c.a2),
                  "coefficients must be finite across the sweep");

            dsp::BandpassBiquad bp;
            bool blew = false;
            for (int n = 0; n < 4800; ++n)
            {
                const float x = sinf(2.0f * kPi * fc * static_cast<float>(n) / kFs);
                const float y = bp.process(x, c);
                if (!std::isfinite(y) || std::fabs(y) > 100.0f) { blew = true; break; }
            }
            check(!blew, "state must stay bounded across the full fc/Q sweep");
        }
    }

    // Reset returns to a clean, reproducible state.
    {
        const dsp::BiquadCoeffs c = dsp::rbjBandpassCoeffs(1000.0f, 5.0f, kFs);
        dsp::BandpassBiquad bp;
        for (int i = 0; i < 200; ++i) { bp.process(1.0f, c); }
        bp.reset();
        const float afterReset = bp.process(0.0f, c);
        check(afterReset == 0.0f, "reset() then process(0) should be exactly 0");

        dsp::BandpassBiquad fresh;
        float a = 0.0f, b = 0.0f;
        for (int i = 0; i < 100; ++i) { a = bp.process(0.3f, c); }
        for (int i = 0; i < 100; ++i) { b = fresh.process(0.3f, c); }
        check(std::fabs(a - b) < 1e-6f, "two instances driven identically should match after reset");
    }

    // DC (fc near 0) and near-Nyquist should not explode.
    {
        const dsp::BiquadCoeffs dcCoeffs = dsp::rbjBandpassCoeffs(1.0f, 1.0f, kFs);
        dsp::BandpassBiquad bp;
        float y = 0.0f;
        for (int i = 0; i < 1000; ++i) { y = bp.process(1.0f, dcCoeffs); }
        check(std::isfinite(y), "near-DC coefficients should stay finite");

        const dsp::BiquadCoeffs nyqCoeffs = dsp::rbjBandpassCoeffs(kFs * 0.49f, 1.0f, kFs);
        dsp::BandpassBiquad bpNyq;
        bool blew = false;
        for (int i = 0; i < 1000; ++i)
        {
            const float x = (i % 2 == 0) ? 1.0f : -1.0f; // Nyquist-rate square wave
            const float out = bpNyq.process(x, nyqCoeffs);
            if (!std::isfinite(out)) { blew = true; break; }
        }
        check(!blew, "near-Nyquist coefficients should stay finite");
    }

    if (failed == 0) { std::printf("biquad_test: PASS\n"); return 0; }
    std::fprintf(stderr, "biquad_test: %d FAIL\n", failed);
    return 1;
}
