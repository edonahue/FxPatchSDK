#!/usr/bin/env python3
"""Compile and probe every custom effect patch with repeatable host-side metrics.

This script is intentionally lightweight:
  - compiles tests/effect_probe.cpp against each top-level effects/*.cpp patch
  - runs the probe at nominal and lower input scales
  - writes machine-readable summaries under build/effect_analysis/

The output is meant to support patch tuning decisions, not replace hardware listening.
"""

from __future__ import annotations

import json
import pathlib
import shutil
import subprocess
import sys
from datetime import datetime, timezone


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD_ROOT = REPO_ROOT / "build" / "effect_analysis"
PROBE_DIR = BUILD_ROOT / "probes"
SUMMARY_JSON = BUILD_ROOT / "summary.json"
SUMMARY_MD = BUILD_ROOT / "summary.md"

LOW_INPUT_SCALE = 0.35
NOMINAL_INPUT_SCALE = 1.0

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

PATCH_METADATA = {
    "tube_screamer": {
        "category": "drive",
        "priority": "critical",
        "identity": "mid-hump overdrive",
        "main_param": 0,
        "level_param": 1,
        "level_role": "output",
        "secondary_param": 2,
        "hold_note": "TS808 / TS9 family toggle",
    },
    "klon_centaur": {
        "category": "drive",
        "priority": "critical",
        "identity": "transparent boost/overdrive",
        "main_param": 0,
        "level_param": 2,
        "level_role": "output",
        "secondary_param": 1,
        "hold_note": "stock / tone-mod toggle",
    },
    "big_muff": {
        "category": "drive",
        "priority": "important",
        "identity": "scooped fuzz/sustain",
        "main_param": 0,
        "level_param": 2,
        "level_role": "blend",
        "secondary_param": 1,
        "hold_note": "tone-bypass mids-lift toggle",
    },
    "mxr_distortion_plus": {
        "category": "drive",
        "priority": "important",
        "identity": "simple Distortion+ style clipper",
        "main_param": 0,
        "level_param": 2,
        "level_role": "output",
        "secondary_param": 1,
        "hold_note": "no alternate mode",
    },
    "bbe_sonic_stomp": {
        "category": "drive-adjacent",
        "priority": "important",
        "identity": "enhancer / exciter",
        "main_param": 1,
        "level_param": 2,
        "level_role": "balance",
        "secondary_param": 0,
        "hold_note": "enhancer / doubler toggle",
    },
    "back_talk_reverse_delay": {
        "category": "time",
        "priority": "secondary",
        "identity": "reverse delay",
        "main_param": 2,
        "level_param": 1,
        "level_role": "feedback",
        "secondary_param": 0,
        "hold_note": "texture layering toggle",
    },
    "phase_90": {
        "category": "modulation",
        "priority": "secondary",
        "identity": "4-stage phaser",
        "main_param": 1,
        "level_param": 2,
        "level_role": "mirror",
        "secondary_param": 0,
        "hold_note": "block / script toggle",
    },
    "chorus": {
        "category": "modulation",
        "priority": "secondary",
        "identity": "stereo chorus",
        "main_param": 0,
        "level_param": 2,
        "level_role": "mix",
        "secondary_param": 1,
        "hold_note": "hold is the same bypass toggle as press",
        "hold_is_bypass": True,
    },
    "wah": {
        "category": "filter",
        "priority": "secondary",
        "identity": "Crybaby / Vox wah",
        "main_param": 2,
        "level_param": 0,
        "level_role": "mix",
        "secondary_param": 1,
        "hold_note": "Crybaby / Vox toggle",
    },
}


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )


def safe_div(num: float, den: float) -> float:
    if abs(den) <= 1.0e-12:
        return 0.0
    return num / den


def patch_files() -> list[pathlib.Path]:
    return sorted((REPO_ROOT / "effects").glob("*.cpp"))


def build_probe(effect_path: pathlib.Path) -> pathlib.Path:
    PROBE_DIR.mkdir(parents=True, exist_ok=True)
    output_path = PROBE_DIR / effect_path.stem
    compile_cmd = [
        *COMPILE_FLAGS,
        "tests/effect_probe.cpp",
        str(effect_path.relative_to(REPO_ROOT)),
        "-o",
        str(output_path),
    ]
    result = run(compile_cmd)
    if result.stderr:
        # Keep compiler warnings visible without making them fatal.
        sys.stderr.write(result.stderr)
    return output_path


def run_probe(binary_path: pathlib.Path, patch_name: str, input_scale: float) -> dict:
    completed = run([str(binary_path), patch_name, f"{input_scale:.6f}"])
    return json.loads(completed.stdout)


def series_values(run_data: dict, param_key: str, metric_path: tuple[str, ...]) -> list[float]:
    values = []
    for point in run_data["sweeps"][param_key]:
        node = point["metrics"]
        for key in metric_path:
            node = node[key]
        values.append(float(node))
    return values


def value_span(values: list[float]) -> float:
    if not values:
        return 0.0
    return max(values) - min(values)


def level_ratio(values: list[float]) -> float:
    filtered = [v for v in values if v > 1.0e-9]
    if not filtered:
        return 0.0
    return max(filtered) / min(filtered)


def closest_sweep_point(points: list[dict], metric_path: tuple[str, ...], reference: float) -> dict | None:
    if not points:
        return None

    best_point: dict | None = None
    best_error = float("inf")
    for point in points:
        node = point["metrics"]
        for key in metric_path:
            node = node[key]
        value = float(node)
        error = abs(value - reference)
        if error < best_error:
            best_error = error
            best_point = point

    return best_point


def monotonic_score(values: list[float]) -> float:
    if len(values) < 2:
        return 1.0

    diffs = [values[i + 1] - values[i] for i in range(len(values) - 1)]
    nonzero = [d for d in diffs if abs(d) > 1.0e-6]
    if not nonzero:
        return 1.0

    positive = sum(1 for d in nonzero if d > 0.0)
    negative = sum(1 for d in nonzero if d < 0.0)
    return max(positive, negative) / len(nonzero)


def describe_drive_flags(derived: dict) -> list[str]:
    flags: list[str] = []
    if derived["low_input_delta"] < 0.35 and derived["low_input_residual"] < 0.12:
        flags.append("near-bypass at lower input")
    if derived["main_residual_span_low"] < 0.08:
        flags.append("gain sweep has narrow nonlinear span")
    if derived["main_burst_delta_span_low"] < 0.20:
        flags.append("gain sweep has narrow audible-change span")
    if derived["level_ratio_nominal"] < 1.6:
        flags.append("level/output control authority is limited")
    if derived["expects_centered_output_unity"] and (
        derived["unity_value_nominal"] < 0.35 or derived["unity_value_nominal"] > 0.65
    ):
        flags.append("unity point is poorly centered on the output control")
    if derived["expects_centered_output_unity"] and derived["hot_ratio_at_unity_nominal"] > 0.08:
        flags.append("unity point is already spending too much time near the limiter")
    if derived["expects_centered_output_unity"] and derived["clip_ratio_at_unity_nominal"] > 0.01:
        flags.append("unity point is clipping against the digital ceiling")
    if derived["hold_diff_nominal"] < 0.10:
        flags.append("hold mode delta is subtle")
    return flags


def severity_score(meta: dict, low_run: dict, nominal_run: dict) -> tuple[int, list[str]]:
    category = meta["category"]
    derived = derive_patch_summary(meta, low_run, nominal_run)
    flags: list[str] = []
    score = 0

    if category == "drive":
        flags = describe_drive_flags(derived)
        score += len(flags)
        if derived["low_input_delta"] < 0.25 and derived["low_input_residual"] < 0.08:
            score += 2
        if derived["main_residual_span_low"] < 0.05:
            score += 1
    elif category == "drive-adjacent":
        if derived["low_input_delta"] < 0.18:
            flags.append("effect is intentionally subtle at lower input")
            score += 1
        if derived["hold_diff_nominal"] >= 0.15:
            flags.append("hold mode is meaningfully different")
    elif category == "time":
        if derived["tail_rms_nominal"] < 0.01:
            flags.append("tail energy is weaker than expected for a delay")
            score += 1
        if derived["hold_diff_nominal"] >= 0.10:
            flags.append("hold mode produces a clear alternate texture")
    elif category == "modulation":
        if meta.get("hold_is_bypass"):
            pass
        elif derived["hold_diff_nominal"] >= 0.08:
            flags.append("hold mode produces a measurable alternate modulation voice")
    elif category == "filter":
        if derived["hold_diff_nominal"] >= 0.08:
            flags.append("mode change is clearly audible")

    return score, flags


def derive_patch_summary(meta: dict, low_run: dict, nominal_run: dict) -> dict:
    main_param = f"param{meta['main_param']}"
    level_param = f"param{meta['level_param']}"
    secondary_param = f"param{meta['secondary_param']}"

    main_residual_low = series_values(low_run, main_param, ("sine", "residual_ratio"))
    main_delta_low_burst = series_values(low_run, main_param, ("burst", "delta_ratio"))
    main_delta_low = series_values(low_run, main_param, ("sine", "delta_ratio"))
    level_rms_nominal = series_values(nominal_run, level_param, ("burst", "output_midband_rms"))
    mix_rms_nominal = series_values(nominal_run, secondary_param, ("burst", "output_rms"))
    level_peak_nominal = series_values(nominal_run, level_param, ("burst", "peak_abs"))
    level_hot_nominal = series_values(nominal_run, level_param, ("burst", "hot_sample_ratio"))
    level_clip_nominal = series_values(nominal_run, level_param, ("burst", "clip_sample_ratio"))
    bypass_midband_nominal = float(nominal_run["bypassed"]["burst"]["output_midband_rms"])
    unity_point = closest_sweep_point(
        nominal_run["sweeps"][level_param],
        ("burst", "output_midband_rms"),
        bypass_midband_nominal,
    )

    unity_value = 0.0
    unity_midband = 0.0
    unity_hot = 0.0
    unity_clip = 0.0
    if unity_point is not None:
        unity_value = float(unity_point["value"])
        unity_midband = float(unity_point["metrics"]["burst"]["output_midband_rms"])
        unity_hot = float(unity_point["metrics"]["burst"]["hot_sample_ratio"])
        unity_clip = float(unity_point["metrics"]["burst"]["clip_sample_ratio"])

    return {
        "low_input_delta": float(low_run["active"]["burst"]["delta_ratio"]),
        "low_input_residual": float(low_run["active"]["sine"]["residual_ratio"]),
        "nominal_input_delta": float(nominal_run["active"]["burst"]["delta_ratio"]),
        "nominal_input_residual": float(nominal_run["active"]["sine"]["residual_ratio"]),
        "bypass_delta_nominal": float(nominal_run["bypassed"]["burst"]["delta_ratio"]),
        "hold_diff_nominal": max(
            float(nominal_run["hold_mode_diff"]["burst_diff_ratio"]),
            float(nominal_run["hold_mode_diff"]["sine_diff_ratio"]),
        ),
        "expects_centered_output_unity": meta.get("level_role") == "output",
        "tail_rms_nominal": float(nominal_run["active"]["burst"]["tail_rms"]),
        "main_residual_span_low": value_span(main_residual_low),
        "main_burst_delta_span_low": value_span(main_delta_low_burst),
        "main_sine_delta_span_low": value_span(main_delta_low),
        "main_residual_monotonic_low": monotonic_score(main_residual_low),
        "main_burst_delta_monotonic_low": monotonic_score(main_delta_low_burst),
        "level_ratio_nominal": level_ratio(level_rms_nominal),
        "level_monotonic_nominal": monotonic_score(level_rms_nominal),
        "level_peak_max_nominal": max(level_peak_nominal) if level_peak_nominal else 0.0,
        "level_hot_max_nominal": max(level_hot_nominal) if level_hot_nominal else 0.0,
        "level_clip_max_nominal": max(level_clip_nominal) if level_clip_nominal else 0.0,
        "unity_value_nominal": unity_value,
        "unity_midband_error_nominal": safe_div(abs(unity_midband - bypass_midband_nominal), bypass_midband_nominal),
        "hot_ratio_at_unity_nominal": unity_hot,
        "clip_ratio_at_unity_nominal": unity_clip,
        "mix_span_nominal": value_span(mix_rms_nominal),
    }


def summarize_patch(effect_name: str, low_run: dict, nominal_run: dict) -> dict:
    meta = PATCH_METADATA.get(effect_name)
    if meta is None:
        raise KeyError(f"Missing metadata for patch {effect_name}")

    derived = derive_patch_summary(meta, low_run, nominal_run)
    score, flags = severity_score(meta, low_run, nominal_run)

    return {
        "name": effect_name,
        "category": meta["category"],
        "priority": meta["priority"],
        "identity": meta["identity"],
        "hold_note": meta["hold_note"],
        "low_input": low_run,
        "nominal_input": nominal_run,
        "derived": derived,
        "severity_score": score,
        "flags": flags,
    }


def ranking_key(summary: dict) -> tuple[int, int, str]:
    priority_order = {
        "critical": 0,
        "important": 1,
        "secondary": 2,
    }
    return (
        priority_order.get(summary["priority"], 3),
        -summary["severity_score"],
        summary["name"],
    )


def write_markdown(summaries: list[dict]) -> None:
    lines = [
        "# Effect Analysis Summary",
        "",
        f"Generated: {datetime.now(timezone.utc).isoformat()}",
        "",
        f"Input scales: low={LOW_INPUT_SCALE:.2f}, nominal={NOMINAL_INPUT_SCALE:.2f}",
        "",
        "| Patch | Category | Priority | Low input delta | Low input residual | Main span | Level ratio | Unity | Hold diff | Flags |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|---|",
    ]

    for summary in summaries:
        d = summary["derived"]
        lines.append(
            "| "
            + f"{summary['name']} | "
            + f"{summary['category']} | "
            + f"{summary['priority']} | "
            + f"{d['low_input_delta']:.3f} | "
            + f"{d['low_input_residual']:.3f} | "
            + f"{d['main_residual_span_low']:.3f} | "
            + f"{d['level_ratio_nominal']:.2f} | "
            + f"{d['unity_value_nominal']:.2f} | "
            + f"{d['hold_diff_nominal']:.3f} | "
            + (", ".join(summary["flags"]) if summary["flags"] else "none")
            + " |"
        )

    SUMMARY_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    if shutil.which("g++") is None:
        print("error: g++ is required", file=sys.stderr)
        return 1

    BUILD_ROOT.mkdir(parents=True, exist_ok=True)

    summaries: list[dict] = []
    for effect_path in patch_files():
        binary_path = build_probe(effect_path)
        patch_name = effect_path.stem
        low_run = run_probe(binary_path, patch_name, LOW_INPUT_SCALE)
        nominal_run = run_probe(binary_path, patch_name, NOMINAL_INPUT_SCALE)
        summaries.append(summarize_patch(patch_name, low_run, nominal_run))

    ordered = sorted(summaries, key=ranking_key)

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "input_scales": {
            "low": LOW_INPUT_SCALE,
            "nominal": NOMINAL_INPUT_SCALE,
        },
        "patches": ordered,
    }

    SUMMARY_JSON.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    write_markdown(ordered)

    print("=== Custom Effect Analysis ===")
    print(f"summary.json: {SUMMARY_JSON.relative_to(REPO_ROOT)}")
    print(f"summary.md:   {SUMMARY_MD.relative_to(REPO_ROOT)}")
    print("")
    print(
        f"{'patch':24} {'cat':12} {'lowΔ':>7} {'lowRes':>7} {'mainSpan':>9} "
        f"{'lvl×':>6} {'unity':>6} {'holdΔ':>7} flags"
    )
    for summary in ordered:
        d = summary["derived"]
        flags = ", ".join(summary["flags"]) if summary["flags"] else "none"
        print(
            f"{summary['name']:24} "
            f"{summary['category'][:12]:12} "
            f"{d['low_input_delta']:7.3f} "
            f"{d['low_input_residual']:7.3f} "
            f"{d['main_residual_span_low']:9.3f} "
            f"{d['level_ratio_nominal']:6.2f} "
            f"{d['unity_value_nominal']:6.2f} "
            f"{d['hold_diff_nominal']:7.3f} "
            f"{flags}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
