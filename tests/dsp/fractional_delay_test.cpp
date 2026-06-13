// tests/dsp/fractional_delay_test.cpp — unit tests for dsp::lerpRead / lerpReadPow2.

#include "dsp/fractional_delay.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    const float buf[8] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};

    // Integer positions return the sample at that index.
    for (int i = 0; i < 8; ++i)
    {
        const float v = dsp::lerpRead(buf, 8, (float) i);
        check(v == buf[i], "integer position should return the indexed sample");
    }

    // Fractional positions linearly interpolate.
    const float mid = dsp::lerpRead(buf, 8, 1.5f);
    check(std::fabs(mid - 1.5f) < 1e-6f, "midpoint should be average of the two surrounding samples");

    const float threeQuarter = dsp::lerpRead(buf, 8, 2.75f);
    check(std::fabs(threeQuarter - (2.0f * 0.25f + 3.0f * 0.75f)) < 1e-6f, "lerp must be linear in frac");

    // Wraparound: reading at len wraps to 0 -> 0 (the lerp between buf[0] and buf[1] at frac 0).
    const float wrap = dsp::lerpRead(buf, 8, 8.0f);
    check(wrap == buf[0], "wrap should land on buf[0]");

    // Pow2 variant: mask = len - 1; expect identical results for in-range positions.
    for (float pos = 0.0f; pos < 7.0f; pos += 0.123f)
    {
        const float a = dsp::lerpRead(buf, 8, pos);
        const float b = dsp::lerpReadPow2(buf, 7, pos);
        check(std::fabs(a - b) < 1e-6f, "pow2 variant should match generic for in-range positions");
    }

    // Negative readPos is clamped to 0.
    const float neg = dsp::lerpRead(buf, 8, -1.0f);
    check(neg == buf[0], "negative readPos should clamp to 0");

    if (failed == 0) { printf("fractional_delay_test: PASS\n"); return 0; }
    fprintf(stderr, "fractional_delay_test: %d FAIL\n", failed);
    return 1;
}
