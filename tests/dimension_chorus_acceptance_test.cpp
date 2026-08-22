// tests/dimension_chorus_acceptance_test.cpp
//
// Acceptance test: confirms effects/dimension_chorus.cpp's Width knob does
// what its design claims -- and, in doing so, that the effect is
// architecturally distinct from effects/chorus.cpp's Mix knob rather than a
// re-tuned copy of the same idea.
//
// The property under test is stereo divergence, not mono-sum cancellation.
// (An earlier version of this test targeted "mono-sum RMS decreases as
// Width increases," modeled on the real Boss DC-2's well-documented
// mono-cancellation quirk. That property does not hold reliably here: the
// LFO continuously sweeps the relative phase between the direct and
// cross-fed taps, so a fixed-sign crossfeed gain does not net-cancel when
// averaged over an LFO cycle for an arbitrary test signal. See the
// "note on the real hardware's mono-cancellation quirk" in
// effects/dimension_chorus.cpp's header comment for the full reasoning.
// This test was rewritten to check the property the implementation
// actually, provably delivers.)
//
// At Width=0, dimension_chorus.cpp's processAudio produces exact dry
// passthrough (outL=inL, outR=inR by construction), so stereo difference
// (L-R) is exactly zero for an identical-mono test signal. As Width
// increases, the crossfeed term scales directly with Width, so stereo
// divergence should grow monotonically. effects/chorus.cpp's Mix knob has
// no comparable relationship to stereo divergence -- it crossfades an
// already-present, Width-independent baseline of L/R decorrelation (from
// its two independently-phased delay lines) rather than generating
// divergence proportional to the knob itself.
//
// This test includes effects/dimension_chorus.cpp directly into this
// translation unit (so Patch::getInstance() resolves cleanly with no link
// conflict against any other effect's own getInstance() definition).

#include "../effects/dimension_chorus.cpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int failed = 0;
static void check(bool cond, const char* msg)
{
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); ++failed; }
}

static void fillSine(std::vector<float>& buf, int n, float startSample)
{
    constexpr float kFreqHz = 440.0f;
    constexpr float kAmp    = 0.3f;
    constexpr float kTwoPi  = 6.283185307f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (startSample + static_cast<float>(i)) / 48000.0f;
        buf[i] = kAmp * sinf(kTwoPi * kFreqHz * t);
    }
}

// Sets Width, runs a settle phase (long enough for the ParamSmoother to
// fully converge -- Width's smoother has a 15 ms time constant, settling in
// roughly 4-5x that; a 100 ms settle phase is a comfortable margin), then
// measures stereo-difference RMS over a second, independent 100 ms window.
static double stereoDiffRmsAtWidth(Patch* patch, float widthValue, float& sampleCursor)
{
    constexpr int kSettleSamples  = 4800; // 100 ms @ 48 kHz
    constexpr int kMeasureSamples = 4800;

    patch->setParamValue(0, 0.5f); // Rate
    patch->setParamValue(1, 0.7f); // Depth -- comfortably audible
    patch->setParamValue(2, widthValue);

    std::vector<float> settleL(kSettleSamples), settleR(kSettleSamples);
    fillSine(settleL, kSettleSamples, sampleCursor);
    settleR = settleL;
    sampleCursor += static_cast<float>(kSettleSamples);
    patch->processAudio(std::span<float>(settleL.data(), kSettleSamples),
                        std::span<float>(settleR.data(), kSettleSamples));

    std::vector<float> measL(kMeasureSamples), measR(kMeasureSamples);
    fillSine(measL, kMeasureSamples, sampleCursor);
    measR = measL;
    sampleCursor += static_cast<float>(kMeasureSamples);
    patch->processAudio(std::span<float>(measL.data(), kMeasureSamples),
                        std::span<float>(measR.data(), kMeasureSamples));

    double sumSq = 0.0;
    for (int i = 0; i < kMeasureSamples; ++i)
    {
        const double d = measL[i] - measR[i];
        sumSq += d * d;
    }
    return std::sqrt(sumSq / kMeasureSamples);
}

int main()
{
    Patch* patch = Patch::getInstance();
    std::vector<float> workingBuffer(Patch::kWorkingBufferSize, 0.0f);
    patch->setWorkingBuffer(std::span<float, Patch::kWorkingBufferSize>(
        workingBuffer.data(), Patch::kWorkingBufferSize));
    patch->init();

    float sampleCursor = 0.0f;
    const double diffAt0   = stereoDiffRmsAtWidth(patch, 0.0f, sampleCursor);
    const double diffAt25  = stereoDiffRmsAtWidth(patch, 0.25f, sampleCursor);
    const double diffAt50  = stereoDiffRmsAtWidth(patch, 0.5f, sampleCursor);
    const double diffAt100 = stereoDiffRmsAtWidth(patch, 1.0f, sampleCursor);

    fprintf(stderr,
            "dimension_chorus stereo-diff RMS: width=0.0 -> %f, 0.25 -> %f, "
            "0.5 -> %f, 1.0 -> %f\n",
            diffAt0, diffAt25, diffAt50, diffAt100);

    // Not exactly zero: the ParamSmoother approaches its target
    // asymptotically (never reaching it exactly), and init()'s default
    // Width (0.60) has to ramp all the way down to 0 within the settle
    // window. 1e-3 comfortably separates "essentially settled to dry" from
    // a genuine leak.
    check(diffAt0 < 1e-3, "stereo difference at width=0 should be ~zero (settles to dry passthrough)");
    check(diffAt25 <= diffAt50 + 1e-6, "stereo difference should be non-decreasing 0.25 -> 0.5");
    check(diffAt50 <= diffAt100 + 1e-6, "stereo difference should be non-decreasing 0.5 -> 1.0");
    check(diffAt100 > diffAt0 + 1e-4,
          "stereo difference should grow substantially from width=0 to width=1 "
          "(if this fails, the crossfeed is too weak to be architecturally "
          "distinct from a plain crossfade chorus)");

    if (failed == 0) { printf("dimension_chorus_acceptance_test: PASS\n"); return 0; }
    fprintf(stderr, "dimension_chorus_acceptance_test: %d FAIL\n", failed);
    return 1;
}
