// tests/wah_svf_accuracy_probe.cpp -- quantifies the resonant-frequency fix
// applied to effects/wah.cpp: the old Chamberlin SVF (f1 = 2*sin(pi*fc/fs))
// versus the new RBJ constant-peak-gain bandpass biquad
// (dsp::rbjBandpassCoeffs / dsp::BandpassBiquad), across the exact fc/Q
// space wah.cpp's own knobs can reach.
//
// Method: drive each filter's real per-sample recursion with swept sine
// test tones and find the frequency of maximum steady-state output
// amplitude -- the same empirical technique that originally found the bug,
// not a closed-form pole-angle argument (which was shown to disagree with
// direct simulation for this filter's zero structure; see
// source/dsp/filter_coeff.h's warning on svfF1).
//
// This file intentionally reimplements the old Chamberlin recursion inline
// rather than importing it from anywhere -- there is no repo primitive for
// it any more (svfF1 remains, but the flawed *recursion* it fed was only
// ever inlined at each call site, and wah.cpp's own copy is gone after this
// fix). Keeping a standalone copy here is what lets this probe show the
// "before" number forever, not just at the moment of the fix.
//
// Build and run:
//   g++ -std=c++20 -O2 -fsingle-precision-constant -I source
//       tests/wah_svf_accuracy_probe.cpp -o build/wah_svf_accuracy_probe
//   build/wah_svf_accuracy_probe

#include "dsp/biquad.h"
#include "dsp/filter_coeff.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kFs = 48000.0f;
constexpr float kPi = 3.14159265f;

// The old, replaced recursion -- effects/wah.cpp's exact prior math,
// verbatim, kept only so this probe can keep quantifying what changed.
struct OldChamberlinSvf
{
    float low = 0.0f, band = 0.0f;
    float step(float in, float f1, float q1)
    {
        low += f1 * band;
        const float hi = in - low - q1 * band;
        band += f1 * hi;
        return band;
    }
};

float oldPeakHz(float fc, float q, int settleSamples = 3000)
{
    const float f1 = 2.0f * sinf(kPi * fc / kFs);
    const float q1 = 1.0f / q;
    float bestFreq = fc, bestAmp = -1.0f;
    for (float testFc = fc * 0.5f; testFc <= fc * 1.6f; testFc += fc * 0.002f)
    {
        OldChamberlinSvf svf;
        float maxAmp = 0.0f;
        bool blew = false;
        for (int i = 0; i < settleSamples; ++i)
        {
            const float t = static_cast<float>(i) / kFs;
            const float y = svf.step(sinf(2.0f * kPi * testFc * t), f1, q1);
            if (!std::isfinite(y)) { blew = true; break; }
            if (i > settleSamples * 3 / 4) { maxAmp = std::fmax(maxAmp, std::fabs(y)); }
        }
        if (blew) { continue; }
        if (maxAmp > bestAmp) { bestAmp = maxAmp; bestFreq = testFc; }
    }
    return bestFreq;
}

float newPeakHz(float fc, float q, int settleSamples = 3000)
{
    const dsp::BiquadCoeffs c = dsp::rbjBandpassCoeffs(fc, q, kFs);
    float bestFreq = fc, bestAmp = -1.0f;
    for (float testFc = fc * 0.5f; testFc <= fc * 1.6f; testFc += fc * 0.002f)
    {
        dsp::BandpassBiquad bp;
        float maxAmp = 0.0f;
        for (int i = 0; i < settleSamples; ++i)
        {
            const float t = static_cast<float>(i) / kFs;
            const float y = bp.process(sinf(2.0f * kPi * testFc * t), c);
            if (i > settleSamples * 3 / 4) { maxAmp = std::fmax(maxAmp, std::fabs(y)); }
        }
        if (maxAmp > bestAmp) { bestAmp = maxAmp; bestFreq = testFc; }
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
            std::printf("%-8.0f %-6.1f %11.1fHz %+8.1fc %11.1fHz %+8.1fc\n",
                       static_cast<double>(fc), static_cast<double>(q),
                       static_cast<double>(oldPeak), static_cast<double>(oldCents),
                       static_cast<double>(newPeak), static_cast<double>(newCents));
        }
    }

    std::printf("\nworst old error: %.1f cents\n", static_cast<double>(worstOldCents));
    std::printf("worst new error: %.3f cents\n", static_cast<double>(worstNewCents));
    return 0;
}
