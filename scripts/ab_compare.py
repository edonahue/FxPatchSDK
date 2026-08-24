#!/usr/bin/env python3
"""ab_compare.py — generic A/B / null-test comparison between two captures.

Generalizes the *pattern* tests/dimension_chorus_acceptance_test.cpp
hardcoded to one effect via #include: reads two captures (raw float32 +
JSON sidecar, the same convention tests/oversample_alias_probe.cpp and
tests/effect_probe.cpp's spectral capture use) written by
tests/ab_capture_probe.cpp, and reports RMS-diff ratio, correlation, and a
spectral diff via scripts/spectral_fft.py's magnitude_squared_spectrum --
the genuinely new capability beyond what the hardcoded test had.

Two usage modes:

  # Compare two already-captured files directly.
  python3 scripts/ab_compare.py \
      --capture-a build/ab_compare/chorus.f32 \
      --capture-b build/ab_compare/dimension_chorus.f32

  # Build + capture both effects from scratch (the common case), then
  # compare. Shares --param0/1/2, --hold, --signal, --scale across both
  # sides unless a --NN-a/--NN-b per-side override is given.
  python3 scripts/ab_compare.py --effect-a chorus --effect-b dimension_chorus

tests/dimension_chorus_acceptance_test.cpp is not replaced by this tool --
it stays as a cheap, already-verified regression guard for one specific
property. Future comparisons (pre/post-refactor verification, new
sibling-effect differentiation checks) should reach for this tool instead
of writing another one-off #include-based test.
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import subprocess
import sys

import spectral_fft

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_OUT_DIR = REPO_ROOT / "build" / "ab_compare"

COMPILE_FLAGS = [
    "g++",
    "-std=c++20",
    "-O2",
    "-fno-exceptions",
    "-fno-rtti",
    "-fsingle-precision-constant",
    "-Wall",
    "-Wextra",
    "-I",
    "source",
]


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=REPO_ROOT, check=True, capture_output=True, text=True)


def build_and_capture(effect_name: str, out_dir: pathlib.Path, label: str,
                      extra_args: list[str]) -> tuple[pathlib.Path, pathlib.Path]:
    effect_path = REPO_ROOT / "effects" / f"{effect_name}.cpp"
    if not effect_path.exists():
        print(f"error: effect not found: {effect_path}", file=sys.stderr)
        sys.exit(1)

    probe_dir = DEFAULT_OUT_DIR / "probes"
    probe_dir.mkdir(parents=True, exist_ok=True)
    probe_path = probe_dir / effect_name

    compile_cmd = [
        *COMPILE_FLAGS,
        "tests/ab_capture_probe.cpp",
        str(effect_path.relative_to(REPO_ROOT)),
        "-o",
        str(probe_path),
    ]
    result = run(compile_cmd)
    if result.stderr:
        sys.stderr.write(result.stderr)

    out_dir.mkdir(parents=True, exist_ok=True)
    run([str(probe_path), effect_name, str(out_dir), "--label", label, *extra_args])

    return out_dir / f"{label}.f32", out_dir / f"{label}.json"


def read_pair(bin_path: pathlib.Path) -> tuple[list[float], dict]:
    json_path = bin_path.with_suffix(".json")
    sidecar = json.loads(json_path.read_text())
    samples = spectral_fft.read_capture(bin_path)
    return samples, sidecar


def rms(values: list[float]) -> float:
    if not values:
        return 0.0
    return math.sqrt(sum(v * v for v in values) / len(values))


def correlation(a: list[float], b: list[float]) -> float:
    xy = sum(x * y for x, y in zip(a, b))
    xx = sum(x * x for x in a)
    yy = sum(y * y for y in b)
    if xx <= 1.0e-12 or yy <= 1.0e-12:
        return 0.0
    return xy / math.sqrt(xx * yy)


def spectral_diff(a: list[float], b: list[float]) -> dict:
    """Requires len(a) == len(b) and a power of two -- ab_capture_probe.cpp's
    default --capture 16384 satisfies this. If lengths differ (e.g. custom
    --capture on one side only), the spectral diff is skipped rather than
    silently comparing mismatched windows."""
    n = len(a)
    if len(b) != n or n & (n - 1) != 0:
        return {"skipped": True, "reason": "capture lengths differ or aren't a power of two"}

    mags_a = spectral_fft.magnitude_squared_spectrum(a)
    mags_b = spectral_fft.magnitude_squared_spectrum(b)

    energy_a = sum(mags_a)
    energy_b = sum(mags_b)
    energy_ratio_db = (
        10.0 * math.log10(energy_b / energy_a) if energy_a > 0.0 and energy_b > 0.0 else None
    )

    # Correlation between magnitude spectra as a spectral-shape similarity
    # score: 1.0 = identical shape (regardless of absolute level, which
    # energy_ratio_db already covers separately).
    mag_a = [math.sqrt(m) for m in mags_a]
    mag_b = [math.sqrt(m) for m in mags_b]
    shape_correlation = correlation(mag_a, mag_b)

    return {
        "skipped": False,
        "energy_ratio_db": energy_ratio_db,
        "shape_correlation": shape_correlation,
    }


def compare(bin_path_a: pathlib.Path, bin_path_b: pathlib.Path) -> dict:
    samples_a, sidecar_a = read_pair(bin_path_a)
    samples_b, sidecar_b = read_pair(bin_path_b)

    n = min(len(samples_a), len(samples_b))
    if len(samples_a) != len(samples_b):
        print(
            f"warning: capture lengths differ (a={len(samples_a)}, b={len(samples_b)}); "
            f"comparing the first {n} samples of each",
            file=sys.stderr,
        )
    a = samples_a[:n]
    b = samples_b[:n]

    diff = [x - y for x, y in zip(a, b)]
    rms_a = rms(a)
    rms_diff_ratio = rms(diff) / rms_a if rms_a > 1.0e-12 else rms(diff)

    return {
        "a": sidecar_a,
        "b": sidecar_b,
        "compared_samples": n,
        "rms_diff_ratio": rms_diff_ratio,
        "correlation": correlation(a, b),
        "spectral": spectral_diff(a, b),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--capture-a", type=pathlib.Path)
    parser.add_argument("--capture-b", type=pathlib.Path)
    parser.add_argument("--effect-a")
    parser.add_argument("--effect-b")
    parser.add_argument("--param0", help="applies to both sides unless overridden")
    parser.add_argument("--param1", help="applies to both sides unless overridden")
    parser.add_argument("--param2", help="applies to both sides unless overridden")
    parser.add_argument("--hold-a", action="store_true")
    parser.add_argument("--hold-b", action="store_true")
    parser.add_argument("--signal", default="sine", choices=["sine", "burst"])
    parser.add_argument("--scale", default="1.0")
    parser.add_argument("--out-dir", type=pathlib.Path, default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    if args.capture_a and args.capture_b:
        bin_a, bin_b = args.capture_a, args.capture_b
    elif args.effect_a and args.effect_b:
        shared_args = ["--signal", args.signal, "--scale", args.scale]
        for idx, value in enumerate((args.param0, args.param1, args.param2)):
            if value is not None:
                shared_args += [f"--param{idx}", value]

        args_a = list(shared_args)
        if args.hold_a:
            args_a.append("--hold")
        args_b = list(shared_args)
        if args.hold_b:
            args_b.append("--hold")

        # Labels are prefixed with "a_"/"b_" rather than the bare effect
        # name, even though the common case (two different effects) would
        # never collide -- comparing the same effect under two different
        # settings (e.g. --hold-b) is an equally valid use of this tool,
        # and bare effect-name labels would silently overwrite side A's
        # capture with side B's before compare() ever reads it.
        bin_a, _ = build_and_capture(args.effect_a, args.out_dir, f"a_{args.effect_a}", args_a)
        bin_b, _ = build_and_capture(args.effect_b, args.out_dir, f"b_{args.effect_b}", args_b)
    else:
        parser.error("provide either --capture-a/--capture-b or --effect-a/--effect-b")
        return 1

    result = compare(bin_a, bin_b)

    print(f"A: {result['a'].get('patch')} (label={result['a'].get('label')}, "
         f"params={result['a'].get('params')}, hold={result['a'].get('hold')})")
    print(f"B: {result['b'].get('patch')} (label={result['b'].get('label')}, "
         f"params={result['b'].get('params')}, hold={result['b'].get('hold')})")
    print(f"Compared samples: {result['compared_samples']}")
    print(f"RMS diff ratio (vs A): {result['rms_diff_ratio']:.4f}")
    print(f"Correlation:           {result['correlation']:.4f}")

    spectral = result["spectral"]
    if spectral["skipped"]:
        print(f"Spectral diff: skipped ({spectral['reason']})")
    else:
        ratio = spectral["energy_ratio_db"]
        ratio_text = f"{ratio:.2f} dB" if ratio is not None else "n/a"
        print(f"Spectral energy ratio (B/A): {ratio_text}")
        print(f"Spectral shape correlation:  {spectral['shape_correlation']:.4f}")

    out_path = args.out_dir / "ab_compare_result.json"
    args.out_dir.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"\nwrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
