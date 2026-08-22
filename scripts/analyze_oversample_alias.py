#!/usr/bin/env python3
"""analyze_oversample_alias.py — alias-energy analysis for the oversampling experiment.

Reads the raw float32 captures + JSON sidecars produced by
tests/oversample_alias_probe.cpp and reports, per (mode, signal, amplitude):

    alias_energy_ratio_dB = 10 * log10(alias_energy / total_energy)

where alias_energy is the spectral energy outside the bins a correctly
bandlimited process would produce (the fundamental and its harmonics for a
single-tone sweep; the two fundamentals and their intermodulation products
for a two-tone test). Anything else is, by definition, an artifact of the
processing -- naive aliasing or otherwise.

Stdlib only (no numpy) -- matches scripts/analyze_effects.py's convention.
Implements a small radix-2 FFT in pure Python; block length is 8192 (a
power of two), so an O(N log N) FFT runs in well under a second per
capture, unlike an O(N^2) direct DFT which would be impractically slow
here.

Usage:
    python3 scripts/analyze_oversample_alias.py [capture_dir]

capture_dir defaults to build/oversample_experiment (matching the probe's
own default output location).
"""
from __future__ import annotations

import cmath
import json
import math
import pathlib
import struct
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_CAPTURE_DIR = REPO_ROOT / "build" / "oversample_experiment"

MODES = ["1x", "2x_naive", "2x_halfband"]
AMPS = ["mild", "moderate", "hard"]


def read_capture(bin_path: pathlib.Path) -> list[float]:
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


def expected_bins_twotone(bin1: int, bin2: int, n: int, max_order: int = 5) -> set[int]:
    """f1, f2, and their intermodulation/harmonic combinations m*f1 + k*f2
    up to |m| + |k| <= max_order, mapped to the nearest bin (folded to the
    positive-frequency half of the spectrum)."""
    bins = set()
    for m in range(-max_order, max_order + 1):
        for k in range(-max_order, max_order + 1):
            if m == 0 and k == 0:
                continue
            if abs(m) + abs(k) > max_order:
                continue
            idx = m * bin1 + k * bin2
            idx = abs(idx)
            if 0 < idx < n // 2:
                bins.add(idx)
    return bins


def alias_energy_ratio_db(samples: list[float], expected_bins: set[int],
                          bin_tolerance: int = 1) -> float:
    n = len(samples)
    window = hann_window(n)
    windowed = [complex(s * w, 0.0) for s, w in zip(samples, window)]
    spectrum = fft(windowed)

    half = n // 2
    mags_sq = [abs(spectrum[i]) ** 2 for i in range(half)]
    total_energy = sum(mags_sq)
    if total_energy <= 0.0:
        return float("-inf")

    excluded = set()
    for b in expected_bins:
        for off in range(-bin_tolerance, bin_tolerance + 1):
            idx = b + off
            if 0 <= idx < half:
                excluded.add(idx)

    alias_energy = sum(m for i, m in enumerate(mags_sq) if i not in excluded)
    ratio = alias_energy / total_energy
    if ratio <= 0.0:
        return float("-inf")
    return 10.0 * math.log10(ratio)


def main(argv: list[str]) -> int:
    capture_dir = pathlib.Path(argv[1]) if len(argv) > 1 else DEFAULT_CAPTURE_DIR
    if not capture_dir.is_dir():
        print(f"error: capture directory not found: {capture_dir}", file=sys.stderr)
        print("run build/oversample_probe (tests/oversample_alias_probe.cpp) first",
              file=sys.stderr)
        return 1

    n = 8192
    sweep_bin = 1021
    two_bin1, two_bin2 = 853, 1041
    sweep_expected = expected_bins_sweep(sweep_bin, n)
    twotone_expected = expected_bins_twotone(two_bin1, two_bin2, n)

    results = []
    for amp in AMPS:
        for signal_kind, expected in (("sweep", sweep_expected), ("twotone", twotone_expected)):
            row = {"amplitude": amp, "signal": signal_kind}
            for mode in MODES:
                name = f"{mode}_{signal_kind}_{amp}"
                bin_path = capture_dir / f"{name}.f32"
                if not bin_path.exists():
                    row[mode] = None
                    continue
                samples = read_capture(bin_path)
                row[mode] = alias_energy_ratio_db(samples, expected)
            results.append(row)

    print(f"{'signal':8s} {'amp':9s} {'1x (dB)':>10s} {'2x_naive':>10s} "
          f"{'2x_halfband':>12s} {'naive Δ':>9s} {'halfband Δ':>11s}")
    for row in results:
        base = row.get("1x")
        naive = row.get("2x_naive")
        half = row.get("2x_halfband")
        d_naive = (naive - base) if (base is not None and naive is not None) else None
        d_half = (half - base) if (base is not None and half is not None) else None

        def fmt(v):
            return f"{v:10.2f}" if v is not None and v != float("-inf") else "     -inf"

        def fmt_delta(v):
            return f"{v:9.2f}" if v is not None else "      n/a"

        print(f"{row['signal']:8s} {row['amplitude']:9s} {fmt(base):>10s} "
              f"{fmt(naive):>10s} {fmt(half):>12s} {fmt_delta(d_naive):>9s} "
              f"{fmt_delta(d_half):>11s}")

    out_path = capture_dir / "alias_analysis.json"
    out_path.write_text(json.dumps(results, indent=2))
    print(f"\nwrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
