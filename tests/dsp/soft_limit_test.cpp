// tests/dsp/soft_limit_test.cpp — unit tests for dsp::softLimit.

#include "dsp/soft_limit.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // Identity inside the linear region.
    for (float x = -0.90f; x <= 0.90f; x += 0.05f)
    {
        check(dsp::softLimit(x) == x, "linear region must be identity");
    }

    // Symmetry: f(-x) = -f(x).
    for (float x = 0.0f; x <= 5.0f; x += 0.25f)
    {
        const float a =  dsp::softLimit( x);
        const float b = -dsp::softLimit(-x);
        check(std::fabs(a - b) < 1e-6f, "soft-limit must be odd-symmetric");
    }

    // Asymptote: large input -> threshold + 0.1 = 1.0 at default settings.
    const float huge = dsp::softLimit(1e6f);
    check(std::fabs(huge - 1.0f) < 1e-3f, "asymptote should approach +1.0 at default threshold");

    // Bounded output: result must stay in (-1, 1) for any finite input.
    for (float x = -100.0f; x <= 100.0f; x += 0.5f)
    {
        const float y = dsp::softLimit(x);
        check(std::isfinite(y), "output must be finite");
        check(y > -1.001f && y < 1.001f, "output must stay near [-1, 1] envelope");
    }

    // Divisor parameter controls knee softness: smaller divisor -> harder.
    const float yHard = dsp::softLimit(1.0f, 0.90f, 0.10f);
    const float ySoft = dsp::softLimit(1.0f, 0.90f, 0.50f);
    check(yHard > ySoft, "smaller divisor should approach the asymptote faster");

    // Tail parameter controls the asymptote height: threshold + tail.
    const float yT08 = dsp::softLimit(1e6f, 0.92f, 0.24f, 0.08f);
    const float yT10 = dsp::softLimit(1e6f, 0.90f, 0.25f, 0.10f);
    check(std::fabs(yT08 - 1.0f) < 1e-3f, "tail=0.08 + threshold=0.92 should asymptote at +1.0");
    check(std::fabs(yT10 - 1.0f) < 1e-3f, "tail=0.10 + threshold=0.90 should asymptote at +1.0");

    if (failed == 0) { printf("soft_limit_test: PASS\n"); return 0; }
    fprintf(stderr, "soft_limit_test: %d FAIL\n", failed);
    return 1;
}
