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
// Method: drive the actual per-sample recursion with swept sine test tones
// and find the frequency of maximum steady-state output amplitude -- the
// same empirical technique used to find and quantify the Chamberlin SVF bug
// this primitive replaces (see filter_coeff.h's warning on svfF1). Trusting
// the direct simulation over a closed-form pole-angle argument is
// deliberate: for a filter with nontrivial zeros, the peak of |H| and the
// pole angle can diverge, and simulation is what a player's ear actually
// hears.

#include "dsp/biquad.h"
#include "dsp/filter_coeff.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr float kPi = 3.14159265f;

int failed = 0;
void check(bool cond, const char* msg)
{
    if (!cond) { std::fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

// Empirical resonant-peak frequency: sweep test tones, run each to
// steady state, return the frequency with the largest steady-state
// amplitude. Mirrors the technique used to discover the SVF bug.
float measuredPeakHz(float fc, float q, int settleSamples = 2500)
{
    const dsp::BiquadCoeffs coeffs = dsp::rbjBandpassCoeffs(fc, q, kFs);
    float bestFreq = fc;
    float bestAmp = -1.0f;
    for (float testFc = fc * 0.6f; testFc <= fc * 1.5f; testFc += fc * 0.002f)
    {
        dsp::BandpassBiquad bp;
        float maxAmp = 0.0f;
        for (int i = 0; i < settleSamples; ++i)
        {
            const float t = static_cast<float>(i) / kFs;
            const float x = sinf(2.0f * kPi * testFc * t);
            const float y = bp.process(x, coeffs);
            if (i > settleSamples * 3 / 4) { maxAmp = std::fmax(maxAmp, std::fabs(y)); }
        }
        if (maxAmp > bestAmp) { bestAmp = maxAmp; bestFreq = testFc; }
    }
    return bestFreq;
}

} // namespace

int main()
{
    // Resonant-peak accuracy across every current caller's real range.
    // This is the property the whole swap exists for: the Chamberlin SVF
    // measured up to 174 cents of error over the same grid.
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
