// tests/dsp/ring_buffer_test.cpp — unit tests for dsp::RingBuffer.

#include "dsp/ring_buffer.h"

#include <cmath>
#include <cstdio>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

int main()
{
    constexpr int kLen = 8;
    float storage[kLen] = {};
    dsp::RingBuffer rb;
    rb.init(storage, kLen);
    check(rb.length() == kLen, "length should equal init length");
    check(rb.mask()   == kLen - 1, "mask should be length - 1");

    // Write a ramp, read back at integer delays.
    for (int i = 0; i < kLen; ++i) rb.write((float) (i + 1));   // 1..8

    // After writing 8 samples the write head is at 0 again. Most recent is
    // sample 8 (at index 7), one before is 7 (index 6), etc. A delay equal
    // to the buffer length returns the oldest still-resident sample.
    check(rb.readDelayed(1) == 8.0f, "readDelayed(1) is the most recent write");
    check(rb.readDelayed(2) == 7.0f, "readDelayed(2) is one before");
    check(rb.readDelayed(8) == 1.0f, "readDelayed(length) returns the oldest sample still in the buffer");

    // Fractional read: halfway between most recent (8) and one before (7) -> 7.5.
    const float midpoint = rb.readDelayedFrac(1.5f);
    check(std::fabs(midpoint - 7.5f) < 1e-6f, "fractional read should linearly interpolate");

    // reset() zeros and resets the write head.
    rb.reset();
    for (int i = 0; i < kLen; ++i)
    {
        check(rb.readDelayed(i + 1) == 0.0f, "reset should zero storage");
    }

    // After reset, write a single non-zero sample at write index 0.
    rb.write(1.0f);
    check(rb.readDelayed(1) == 1.0f, "post-reset write should be readable at delay 1");
    check(rb.readDelayed(2) == 0.0f, "no other samples should leak in");

    // Multiple instances coexist with disjoint storage.
    float storeA[4] = {};
    float storeB[4] = {};
    dsp::RingBuffer ra; ra.init(storeA, 4);
    dsp::RingBuffer rb2; rb2.init(storeB, 4);
    ra.write(0.25f);
    rb2.write(0.75f);
    check(ra.readDelayed(1) == 0.25f, "instance A should not see instance B");
    check(rb2.readDelayed(1) == 0.75f, "instance B should not see instance A");

    if (failed == 0) { printf("ring_buffer_test: PASS\n"); return 0; }
    fprintf(stderr, "ring_buffer_test: %d FAIL\n", failed);
    return 1;
}
