// tests/dsp/crossfade_test.cpp — unit tests for dsp::equalPower / linear.

#include "dsp/crossfade.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // Endpoints.
    auto m0  = dsp::equalPower(0.0f);
    auto m1  = dsp::equalPower(1.0f);
    auto mid = dsp::equalPower(0.5f);
    check(std::fabs(m0.dry - 1.0f) < 1e-6f && std::fabs(m0.wet) < 1e-6f, "equalPower(0) -> (1, 0)");
    check(std::fabs(m1.dry) < 1e-6f && std::fabs(m1.wet - 1.0f) < 1e-6f, "equalPower(1) -> (0, 1)");
    const float halfRoot2 = 0.70710678f;
    check(std::fabs(mid.dry - halfRoot2) < 1e-5f, "equalPower(0.5) dry should be sqrt(2)/2");
    check(std::fabs(mid.wet - halfRoot2) < 1e-5f, "equalPower(0.5) wet should be sqrt(2)/2");

    // Power-preservation: dry^2 + wet^2 = 1 across the sweep.
    for (int i = 0; i <= 100; ++i)
    {
        const float mix = i / 100.0f;
        auto g = dsp::equalPower(mix);
        const float power = g.dry * g.dry + g.wet * g.wet;
        check(std::fabs(power - 1.0f) < 1e-5f, "equalPower must preserve power");
    }

    // Linear-law: dry + wet = 1 across the sweep.
    for (int i = 0; i <= 100; ++i)
    {
        const float mix = i / 100.0f;
        auto g = dsp::linear(mix);
        check(std::fabs(g.dry + g.wet - 1.0f) < 1e-6f, "linear must sum to 1");
    }

    // Out-of-range mix is clamped, not extrapolated.
    auto under = dsp::equalPower(-1.0f);
    auto over  = dsp::equalPower( 2.0f);
    check(std::fabs(under.dry - 1.0f) < 1e-6f, "equalPower(-1) should clamp to mix=0");
    check(std::fabs(over.wet  - 1.0f) < 1e-6f, "equalPower(2) should clamp to mix=1");

    if (failed == 0) { printf("crossfade_test: PASS\n"); return 0; }
    fprintf(stderr, "crossfade_test: %d FAIL\n", failed);
    return 1;
}
