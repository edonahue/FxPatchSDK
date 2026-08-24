// tests/funk_machine_limiter_probe.cpp -- does funk_machine's output limiter
// ever actually reach its nonlinear region?
//
// After the control chain went libm-free (see funk_machine_approx_probe.cpp),
// the only newlib transcendental left in the effect is the tanhf inside
// dsp::softLimit. Malleus_Fuzz's cascaded x/(1+|x|) is the obvious
// replacement, and this probe exists to decide whether that swap is worth
// making rather than assuming it is.
//
// dsp::softLimit passes |x| <= threshold through untouched, so the tanhf only
// costs anything on overshoot. The question is how often that happens at the
// effect's real operating levels, with sensitivity and resonance at maximum --
// the settings that produce the hottest output.
//
// Result (see docs/funk-machine-envelope-filter-build-walkthrough.md): at and
// below nominal input the limiter never engages at all, so the tanhf is linked
// but never called and the per-sample cost is a single compare. The swap was
// declined on that basis.
//
// Build and run:
//   g++ -std=c++20 -O2 -fsingle-precision-constant -I source
//       tests/funk_machine_limiter_probe.cpp -o build/funk_limiter
//   build/funk_limiter
#include "../effects/funk_machine_envelope_filter.cpp"
#include <cstdio>
#include <cmath>
#include <vector>
namespace {
constexpr float kThreshold = 0.90f;  // the value funk_machine passes
}
int main()
{
    Patch* p = Patch::getInstance();
    p->init();
    // worst case for output level: max sensitivity, max resonance, mid bias
    p->setParamValue(0, 1.0f);
    p->setParamValue(1, 1.0f);
    p->setParamValue(2, 0.5f);

    for (int scaleIdx = 0; scaleIdx < 3; ++scaleIdx) {
        const float scale = (scaleIdx == 0) ? 0.25f : (scaleIdx == 1) ? 1.0f : 4.0f;
        p->init();
        p->setParamValue(0, 1.0f); p->setParamValue(1, 1.0f); p->setParamValue(2, 0.5f);
        long over = 0, total = 0;
        float peak = 0.0f;
        std::vector<float> l(128), r(128);
        for (int blk = 0; blk < 400; ++blk) {
            for (int i = 0; i < 128; ++i) {
                const float t = static_cast<float>(blk * 128 + i) / 48000.0f;
                const float s = scale * std::sin(2.0f * 3.14159265f * 220.0f * t);
                l[i] = s; r[i] = s;
            }
            p->processAudio(std::span<float>(l.data(), l.size()),
                            std::span<float>(r.data(), r.size()));
            for (int i = 0; i < 128; ++i) {
                const float a = std::fabs(l[i]);
                if (a > peak) peak = a;
                if (a > kThreshold) ++over;
                ++total;
            }
        }
        std::printf("  input x%-5.2f  peak_out=%.4f  samples over %.2f: %ld / %ld (%.3f%%)\n",
                    static_cast<double>(scale), static_cast<double>(peak),
                    static_cast<double>(kThreshold), over, total,
                    100.0 * static_cast<double>(over) / static_cast<double>(total));
    }
    return 0;
}
