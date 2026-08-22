#!/usr/bin/env python3
"""spectral_fft.py — shared pure-Python FFT/spectral-analysis primitives.

Extracted from scripts/analyze_oversample_alias.py (the repo's only prior
FFT implementation) so scripts/analyze_effects.py's THD/spectral analysis
and any future A/B spectral-diff tooling can reuse the same, already-proven
FFT rather than duplicating it.

Stdlib only (no numpy) -- matches the rest of this repo's host-tooling
convention. The FFT is a small iterative radix-2 Cooley-Tukey
implementation; block lengths must be a power of two.
"""
from __future__ import annotations

import math
import struct
from pathlib import Path


def read_capture(bin_path: Path) -> list[float]:
    data = bin_path.read_bytes()
    count = len(data) // 4
    return list(struct.unpack(f"<{count}f", data))


def fft(a: list[complex]) -> list[complex]:
    """Iterative radix-2 Cooley-Tukey FFT. len(a) must be a power of two."""
    n = len(a)
    if n & (n - 1) != 0:
        raise ValueError("fft length must be a power of two")

    # Bit-reversal permutation.
    out = a[:]
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            out[i], out[j] = out[j], out[i]

    length = 2
    while length <= n:
        ang = -2.0 * math.pi / length
        wlen = complex(math.cos(ang), math.sin(ang))
        half = length // 2
        for start in range(0, n, length):
            w = complex(1.0, 0.0)
            for k in range(half):
                u = out[start + k]
                v = out[start + k + half] * w
                out[start + k] = u + v
                out[start + k + half] = u - v
                w *= wlen
        length <<= 1
    return out


def hann_window(n: int) -> list[float]:
    return [0.5 - 0.5 * math.cos(2.0 * math.pi * i / (n - 1)) for i in range(n)]


def expected_bins_sweep(bin_index: int, n: int, max_harmonic: int = 12) -> set[int]:
    """Fundamental + harmonics of a single tone, mapped to the nearest bin."""
    bins = set()
    k = 1
    while k * bin_index < n // 2 and k <= max_harmonic:
        bins.add(k * bin_index)
        k += 1
    return bins


def magnitude_squared_spectrum(samples: list[float]) -> list[float]:
    """Hann-window + FFT once, returning |X[k]|^2 for the positive-frequency
    half (bins [0, len(samples)//2))."""
    n = len(samples)
    window = hann_window(n)
    windowed = [complex(s * w, 0.0) for s, w in zip(samples, window)]
    spectrum = fft(windowed)
    half = n // 2
    return [abs(spectrum[i]) ** 2 for i in range(half)]


def spurious_energy_ratio_db(mags_sq: list[float], expected_bins: set[int],
                             bin_tolerance: int = 1) -> float:
    """Energy outside expected_bins (+/- bin_tolerance) relative to total
    spectral energy, in dB. A generalization of the alias-energy-ratio
    metric to any set of "expected" bins -- harmonics of a fundamental for
    THD-style analysis, or the alias-probe's sweep/two-tone expected sets."""
    half = len(mags_sq)
    total_energy = sum(mags_sq)
    if total_energy <= 0.0:
        return float("-inf")

    excluded = set()
    for b in expected_bins:
        for off in range(-bin_tolerance, bin_tolerance + 1):
            idx = b + off
            if 0 <= idx < half:
                excluded.add(idx)

    spurious_energy = sum(m for i, m in enumerate(mags_sq) if i not in excluded)
    ratio = spurious_energy / total_energy
    if ratio <= 0.0:
        return float("-inf")
    return 10.0 * math.log10(ratio)
