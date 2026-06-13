// tests/dsp/dc_blocker_test.cpp — unit tests for dsp::DcBlocker.

#include "dsp/dc_blocker.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // Constant DC signal should decay toward zero.
    dsp::DcBlocker dc;
    dc.setAlpha(0.999f);
    float y = 0.0f;
    for (int i = 0; i < 50000; ++i) y = dc.process(0.5f);
    check(std::fabs(y) < 1e-2f, "DC input should decay near zero after many samples");

    // Impulse passes through largely unchanged on the first sample.
    dsp::DcBlocker dc2;
    const float yImpulse = dc2.process(1.0f);
    check(std::fabs(yImpulse - 0.999f) < 1e-3f, "impulse should ride through near unity at alpha=0.999");

    // reset() returns the filter to its initial state.
    dsp::DcBlocker dc3;
    for (int i = 0; i < 100; ++i) dc3.process(0.5f);
    dc3.reset();
    const float yPostReset = dc3.process(1.0f);
    check(std::fabs(yPostReset - 0.999f) < 1e-3f, "after reset, behavior should match a fresh instance");

    // Output is finite for any finite input.
    dsp::DcBlocker dc4;
    for (int i = 0; i < 1000; ++i)
    {
        const float x = (i % 2) ? 0.7f : -0.7f;
        const float out = dc4.process(x);
        check(std::isfinite(out), "output must be finite");
    }

    if (failed == 0) { printf("dc_blocker_test: PASS\n"); return 0; }
    fprintf(stderr, "dc_blocker_test: %d FAIL\n", failed);
    return 1;
}
