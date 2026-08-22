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

import json
import pathlib
import sys

import spectral_fft

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_CAPTURE_DIR = REPO_ROOT / "build" / "oversample_experiment"

MODES = ["1x", "2x_naive", "2x_halfband"]
AMPS = ["mild", "moderate", "hard"]

read_capture = spectral_fft.read_capture
expected_bins_sweep = spectral_fft.expected_bins_sweep


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
    mags_sq = spectral_fft.magnitude_squared_spectrum(samples)
    return spectral_fft.spurious_energy_ratio_db(mags_sq, expected_bins, bin_tolerance)


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
