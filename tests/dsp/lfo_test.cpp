// tests/dsp/lfo_test.cpp — unit tests for dsp::SineLfo / TriangleLfo.

#include "dsp/lfo.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    // SineLfo: stateless `value` is the standard sine.
    check(std::fabs(dsp::SineLfo::value(0.0f))   < 1e-6f, "sine value at phase 0 is 0");
    check(std::fabs(dsp::SineLfo::value(0.25f) - 1.0f) < 1e-6f, "sine value at phase 0.25 is +1");
    check(std::fabs(dsp::SineLfo::value(0.75f) + 1.0f) < 1e-6f, "sine value at phase 0.75 is -1");

    // SineLfo: tick advances phase and stays in [-1, 1].
    dsp::SineLfo s;
    s.setRateHz(50.0f);  // arbitrary
    s.reset();
    for (int i = 0; i < 100000; ++i)
    {
        const float y = s.tick();
        check(y >= -1.0001f && y <= 1.0001f, "sine output must stay in [-1, 1]");
        check(s.phase() >= 0.0f && s.phase() < 1.0f, "phase must stay in [0, 1)");
    }

    // SineLfo at 1 Hz @ 48 kHz: 48000 ticks ≈ one full cycle back to ~0.
    dsp::SineLfo s2;
    s2.setRateHz(1.0f);
    s2.reset();
    for (int i = 0; i < 48000; ++i) s2.tick();
    check(std::fabs(s2.phase()) < 1e-3f, "1 Hz @ 48 kHz: phase ~ 0 after 48000 ticks");

    // TriangleLfo: stateless value at the four cardinal phases. The harvested
    // shape is y = 1 - 4 * |wrapped - 0.5|, so phase 0 / 1 -> -1 (trough),
    // phase 0.5 -> +1 (peak), phase 0.25 / 0.75 -> 0 (zero crossings).
    check(std::fabs(dsp::TriangleLfo::value(0.0f) + 1.0f) < 1e-6f, "triangle value at 0 is -1");
    check(std::fabs(dsp::TriangleLfo::value(0.5f) - 1.0f) < 1e-6f, "triangle value at 0.5 is +1");
    check(std::fabs(dsp::TriangleLfo::value(0.25f)) < 1e-6f, "triangle value at 0.25 is 0");
    check(std::fabs(dsp::TriangleLfo::value(0.75f)) < 1e-6f, "triangle value at 0.75 is 0");

    // TriangleLfo: bounded in [-1, 1] across a long run.
    dsp::TriangleLfo t;
    t.setRateHz(50.0f);
    t.reset();
    for (int i = 0; i < 100000; ++i)
    {
        const float y = t.tick();
        check(y >= -1.0001f && y <= 1.0001f, "triangle output must stay in [-1, 1]");
    }

    // Reset returns phase to the given value.
    dsp::SineLfo s3;
    s3.setRateHz(10.0f);
    for (int i = 0; i < 1000; ++i) s3.tick();
    s3.reset(0.25f);
    check(s3.phase() == 0.25f, "reset(phase) should set phase");

    if (failed == 0) { printf("lfo_test: PASS\n"); return 0; }
    fprintf(stderr, "lfo_test: %d FAIL\n", failed);
    return 1;
}
