// tests/dsp/filter_coeff_test.cpp — unit tests for dsp::lpCoeff / hpCoeff.
//
// What this covers: endpoint values, monotonicity in fc, finiteness over a
// 20 Hz - 20 kHz sweep, and linear scaling with sample rate. Run from the
// repo root via tests/check_dsp.sh.

#include "dsp/filter_coeff.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // Endpoint bounds.
    const float lp0   = dsp::lpCoeff(0.0f);
    const float lp20k = dsp::lpCoeff(20000.0f);
    check(lp0 == 0.0f,                 "lpCoeff(0) should be 0");
    check(lp20k > 0.5f && lp20k < 1.0f, "lpCoeff(20k) should be in (0.5, 1)");

    const float hp0   = dsp::hpCoeff(0.0f);
    const float hp20k = dsp::hpCoeff(20000.0f);
    check(hp0 == 1.0f,                 "hpCoeff(0) should be 1");
    check(hp20k > 0.0f && hp20k < 0.5f, "hpCoeff(20k) should be in (0, 0.5)");

    // Monotonicity + finiteness on a 200-point log sweep across 20 Hz - 20 kHz.
    float prevLp = lp0;
    float prevHp = hp0;
    for (int i = 0; i <= 200; ++i)
    {
        const float fc = 20.0f * std::pow(1000.0f, i / 200.0f);
        const float lp = dsp::lpCoeff(fc);
        const float hp = dsp::hpCoeff(fc);
        check(std::isfinite(lp) && std::isfinite(hp), "coefficients must be finite");
        if (i > 0)
        {
            check(lp >= prevLp - 1e-7f, "lpCoeff should be non-decreasing in fc");
            check(hp <= prevHp + 1e-7f, "hpCoeff should be non-increasing in fc");
        }
        prevLp = lp; prevHp = hp;
    }

    // Sample-rate scaling: lpCoeff(fc, 96000) should equal lpCoeff(fc/2, 48000).
    const float a = dsp::lpCoeff(2000.0f, 96000.0f);
    const float b = dsp::lpCoeff(1000.0f, 48000.0f);
    check(std::fabs(a - b) < 1e-6f, "lpCoeff should scale linearly with fs");

    if (failed == 0) { printf("filter_coeff_test: PASS\n"); return 0; }
    fprintf(stderr, "filter_coeff_test: %d FAIL\n", failed);
    return 1;
}
