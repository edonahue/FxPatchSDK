// tests/dsp/one_pole_filter_test.cpp — unit tests for dsp::OnePoleLowpass / OnePoleHighpass.

#include "dsp/one_pole_filter.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // --- OnePoleLowpass -----------------------------------------------

    // DC gain is unity: a constant input converges exactly to itself.
    dsp::OnePoleLowpass lp;
    float y = 0.0f;
    for (int i = 0; i < 5000; ++i) y = lp.process(0.7f, 0.05f);
    check(std::fabs(y - 0.7f) < 1e-4f, "lowpass DC gain should be unity");

    // Impulse response: first sample equals alpha, then decays monotonically
    // toward zero (geometric decay with ratio 1-alpha).
    dsp::OnePoleLowpass lp2;
    const float alpha = 0.3f;
    const float y0 = lp2.process(1.0f, alpha);
    check(std::fabs(y0 - alpha) < 1e-6f, "lowpass impulse response at n=0 should equal alpha");
    float prev = y0;
    for (int i = 0; i < 100; ++i)
    {
        const float cur = lp2.process(0.0f, alpha);
        check(cur <= prev + 1e-9f && cur >= 0.0f, "lowpass impulse decay should be monotonic and non-negative");
        prev = cur;
    }
    check(prev < 1e-6f, "lowpass impulse response should decay near zero");

    // alpha = 1 tracks the input instantly (no lag).
    dsp::OnePoleLowpass lp3;
    check(lp3.process(0.42f, 1.0f) == 0.42f, "lowpass alpha=1 should track input exactly");

    // alpha = 0 never moves.
    dsp::OnePoleLowpass lp4;
    lp4.process(0.9f, 0.0f);
    check(lp4.process(0.9f, 0.0f) == 0.0f, "lowpass alpha=0 should never leave zero");

    // reset() returns to a fresh-instance state.
    dsp::OnePoleLowpass lp5;
    for (int i = 0; i < 100; ++i) lp5.process(0.5f, 0.1f);
    lp5.reset();
    dsp::OnePoleLowpass lp6;
    check(lp5.process(0.2f, 0.1f) == lp6.process(0.2f, 0.1f), "lowpass reset() should match a fresh instance");

    // --- OnePoleHighpass ------------------------------------------------

    // DC gain is zero: a constant input decays to zero after the transient.
    dsp::OnePoleHighpass hp;
    float yh = 0.0f;
    for (int i = 0; i < 5000; ++i) yh = hp.process(0.7f, 0.05f);
    check(std::fabs(yh) < 1e-3f, "highpass DC gain should be zero (constant input decays out)");

    // Impulse response is finite and decays toward zero.
    dsp::OnePoleHighpass hp2;
    hp2.process(1.0f, 0.3f);
    float last = 0.0f;
    for (int i = 0; i < 200; ++i) last = hp2.process(0.0f, 0.3f);
    check(std::isfinite(last), "highpass output must stay finite");
    check(std::fabs(last) < 1e-6f, "highpass impulse response should decay near zero");

    // reset() returns to a fresh-instance state.
    dsp::OnePoleHighpass hp3;
    for (int i = 0; i < 100; ++i) hp3.process(0.5f, 0.1f);
    hp3.reset();
    dsp::OnePoleHighpass hp4;
    check(hp3.process(0.2f, 0.1f) == hp4.process(0.2f, 0.1f), "highpass reset() should match a fresh instance");

    // Output is finite for a bipolar alternating input at various alphas.
    dsp::OnePoleHighpass hp5;
    for (int i = 0; i < 1000; ++i)
    {
        const float x = (i % 2) ? 0.8f : -0.8f;
        check(std::isfinite(hp5.process(x, 0.4f)), "highpass output must stay finite under alternating input");
    }

    if (failed == 0) { printf("one_pole_filter_test: PASS\n"); return 0; }
    fprintf(stderr, "one_pole_filter_test: %d FAIL\n", failed);
    return 1;
}
