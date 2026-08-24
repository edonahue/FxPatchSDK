// tests/dsp/clamp_test.cpp — coverage for dsp::clamp01 / clampSigned / clampUnit.

#include "dsp/clamp.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

bool nearlyEqual(float a, float b, float tol = 1e-6f)
{
    return std::fabs(a - b) <= tol;
}

void testClamp01()
{
    assert(nearlyEqual(dsp::clamp01(-1.0f), 0.0f));
    assert(nearlyEqual(dsp::clamp01(0.0f), 0.0f));
    assert(nearlyEqual(dsp::clamp01(0.5f), 0.5f));
    assert(nearlyEqual(dsp::clamp01(1.0f), 1.0f));
    assert(nearlyEqual(dsp::clamp01(2.0f), 1.0f));

    // In-range values must pass through untouched, not merely close -- these
    // sit on parameter paths where a rounding change would be audible.
    for (int i = 0; i <= 100; ++i) {
        const float v = static_cast<float>(i) / 100.0f;
        assert(dsp::clamp01(v) == v);
    }
}

void testClampSigned()
{
    assert(nearlyEqual(dsp::clampSigned(-3.0f, 1.8f), -1.8f));
    assert(nearlyEqual(dsp::clampSigned(3.0f, 1.8f), 1.8f));
    assert(nearlyEqual(dsp::clampSigned(0.25f, 1.8f), 0.25f));
    assert(dsp::clampSigned(0.0f, 1.8f) == 0.0f);

    assert(nearlyEqual(dsp::clampUnit(-2.0f), -1.0f));
    assert(nearlyEqual(dsp::clampUnit(2.0f), 1.0f));
    assert(dsp::clampUnit(0.5f) == 0.5f);
}

void testInfinities()
{
    const float inf = std::numeric_limits<float>::infinity();
    assert(dsp::clamp01(inf) == 1.0f);
    assert(dsp::clamp01(-inf) == 0.0f);
    assert(dsp::clampUnit(inf) == 1.0f);
    assert(dsp::clampUnit(-inf) == -1.0f);
}

void testNaNIsBounded()
{
    // Documented, deliberate difference from the if-chain helpers these
    // replaced: those propagated NaN, these contain it. See the header.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    assert(!std::isnan(dsp::clamp01(nan)));
    assert(dsp::clamp01(nan) >= 0.0f && dsp::clamp01(nan) <= 1.0f);
    assert(!std::isnan(dsp::clampUnit(nan)));
}

} // namespace

int main()
{
    testClamp01();
    testClampSigned();
    testInfinities();
    testNaNIsBounded();
    std::printf("clamp_test: PASS\n");
    return 0;
}
