// tests/dsp/parameter_smoother_test.cpp — unit tests for dsp::ParamSmoother.

#include "dsp/parameter_smoother.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // After init(value, time), current and target are both `value`.
    dsp::ParamSmoother s;
    s.init(0.5f, 10.0f);
    check(s.current() == 0.5f, "init should set current to value");
    check(s.process() == 0.5f, "process at target should return target");

    // Convergence: stepping the target should asymptote toward it.
    s.setTarget(1.0f);
    float y = 0.0f;
    for (int i = 0; i < 50000; ++i) y = s.process();
    check(std::fabs(y - 1.0f) < 1e-3f, "smoother should converge to target");

    // Monotonic approach (no overshoot) on a step.
    dsp::ParamSmoother s2;
    s2.init(0.0f, 10.0f);
    s2.setTarget(1.0f);
    float prev = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        const float cur = s2.process();
        check(cur >= prev - 1e-6f && cur <= 1.0f + 1e-6f, "single-pole approach should be monotonic");
        prev = cur;
    }

    // snap() jumps to a value immediately (no glide).
    dsp::ParamSmoother s3;
    s3.init(0.0f, 100.0f);
    s3.setTarget(1.0f);
    s3.process();
    s3.snap(0.25f);
    check(s3.current() == 0.25f, "snap should set current immediately");
    check(s3.process() == 0.25f, "snap should also park target so the next process is stationary");

    // processTo combines setTarget + process.
    dsp::ParamSmoother s4;
    s4.init(0.0f, 5.0f);
    const float a = s4.processTo(1.0f);
    dsp::ParamSmoother s5;
    s5.init(0.0f, 5.0f);
    s5.setTarget(1.0f);
    const float b = s5.process();
    check(a == b, "processTo should equal setTarget+process");

    if (failed == 0) { printf("parameter_smoother_test: PASS\n"); return 0; }
    fprintf(stderr, "parameter_smoother_test: %d FAIL\n", failed);
    return 1;
}
