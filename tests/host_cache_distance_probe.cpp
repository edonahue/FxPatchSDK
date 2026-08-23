// tests/host_cache_distance_probe.cpp
//
// Standalone experiment: does reading further behind a moving "write head"
// in a large ring buffer cost measurably more on THIS HOST, as a sanity
// check before real Cortex-M7 hardware data exists for the external-RAM
// latency inferred in docs/patch-authoring-best-practices.md §6? Measures,
// does not assume -- see docs/back-talk-reverse-delay-external-ram-probe.md
// for the results and the (explicitly limited) interpretation.
//
// This file is deliberately NOT under effects/ (so tests/check_patches.sh's
// effects/*.cpp glob never picks it up as a patch) and does not modify
// effects/back_talk_reverse_delay.cpp in any way -- 100% additive tooling,
// following the same pattern as tests/oversample_alias_probe.cpp.
//
// Usage:
//   g++ -std=c++20 -O2 -fno-exceptions -fno-rtti -fsingle-precision-constant
//       tests/host_cache_distance_probe.cpp -o build/host_cache_distance_probe
//   build/host_cache_distance_probe

#include <cstdint>
#include <cstdio>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define CACHE_PROBE_HAVE_RDTSC 1
#endif

namespace {

// Matches effects/back_talk_reverse_delay.cpp's kDelayLen exactly, so the
// buffer size (and hence its relationship to real cache sizes) is the same
// one the actual patch uses, not an arbitrary round number.
constexpr int kDelayLen = 131072;

// Distances to probe, in samples behind the write head: near (matches the
// "100 ms" example already used in patch-authoring-best-practices.md),
// back_talk_reverse_delay.cpp's own maximum reverse-chunk reach (~1.2s),
// one point in between, and the largest distance the buffer allows at all
// (kDelayLen - 1) as a sanity check that the probe would detect *something*
// if the host's cache hierarchy ever ran out of room for this buffer size.
constexpr int kDistances[] = {4800, 24000, 57600, kDelayLen - 1};
constexpr int kTrialsPerDistance = 50;
constexpr int kReadsPerTrial = 4096;  // amortize rdtsc overhead across many reads

#ifdef CACHE_PROBE_HAVE_RDTSC
uint64_t rdtscMin(int writeHead, int distance, std::vector<float>& buffer)
{
    uint64_t best = UINT64_MAX;
    const int mask = kDelayLen - 1;  // kDelayLen is a power of two
    volatile float sink = 0.0f;      // prevents the compiler from eliding the reads

    for (int trial = 0; trial < kTrialsPerDistance; ++trial) {
        const uint64_t start = __rdtsc();
        int readPos = (writeHead - distance) & mask;
        for (int i = 0; i < kReadsPerTrial; ++i) {
            sink = buffer[readPos];
            // Advance both pointers together, exactly like a real delay
            // line's write head and a fixed-offset reverse tap would.
            readPos = (readPos + 1) & mask;
        }
        const uint64_t elapsed = __rdtsc() - start;
        if (elapsed < best) best = elapsed;
    }

    (void) sink;
    return best;
}
#endif

}  // namespace

int main()
{
#ifndef CACHE_PROBE_HAVE_RDTSC
    std::fprintf(stderr, "rdtsc not available on this platform; nothing to measure.\n");
    return 1;
#else
    static_assert((kDelayLen & (kDelayLen - 1)) == 0, "kDelayLen must be a power of two");

    // A real delay line is continuously written, not a static array -- fill
    // it so every distance reads real, resident data rather than
    // just-zeroed pages.
    std::vector<float> buffer(kDelayLen);
    for (int i = 0; i < kDelayLen; ++i) {
        buffer[i] = static_cast<float>(i);
    }

    const int writeHead = kDelayLen / 2;  // arbitrary fixed position

    std::printf("%-12s %-20s %-16s\n", "distance", "min cycles/trial", "cycles/read");
    for (int distance : kDistances) {
        const uint64_t minCycles = rdtscMin(writeHead, distance, buffer);
        const double perRead = static_cast<double>(minCycles) / kReadsPerTrial;
        std::printf("%-12d %-20llu %-24.2f\n", distance,
                   static_cast<unsigned long long>(minCycles), perRead);
    }

    return 0;
#endif
}
