#!/usr/bin/env python3
"""Static analysis of compiled .endl patch images.

Answers questions that only the binary can answer, for patches whose source we
do not have: which library routines a patch links, how much of its image is
library code versus effect code, how float-dense the code is, and which ABI
slots it actually implements.

Third-party .endl files carry no symbols. To identify library routines in them,
this script builds fingerprints from *our* builds -- where `build/<name>.elf`
gives ground-truth symbol addresses and sizes -- and matches those fingerprints
against unlabeled images. A fingerprint is the set of 4-byte words appearing in
a routine's body in every one of our builds, minus words that show up in more
than two different routines. Literal-pool constants dominate, and those are
position-independent, so the fingerprint survives the routine landing at a
different address in a different image.

The technique is validated by construction: run it against our own binaries and
compare with `arm-none-eabi-nm`. See docs/endl-corpus-study.md.

Usage:
    python3 scripts/endl_analyze.py                     # whole repo corpus
    python3 scripts/endl_analyze.py PATH...             # specific images
    python3 scripts/endl_analyze.py --rebuild-signatures
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from endl_inspect import (  # noqa: E402
    HEADER_SIZE, DEFAULT_LOAD_ADDR, parse_header, validate, classify_entries,
)

REPO = Path(__file__).resolve().parent.parent
OUT_DIR = REPO / "build" / "endl_analysis"
SIG_PATH = OUT_DIR / "libm_signatures.json"

# Corpus groups. Third-party images are inputs only -- this repo claims no
# rights over them (see playground/polyend_plates/README.md) and the analysis
# below is structural and statistical, never a transcription of their DSP.
GROUPS = [
    ("polyend_factory", REPO / "playground" / "polyend_plates"),
    ("playground_community", REPO / "playground" / "examples" / "SpiralCaster_Examples"),
    ("this_repo", REPO / "effects" / "builds"),
]

# Routine-name classification for the "what is this image made of" split.
LIBM_PREFIXES = (
    "sinf", "cosf", "tanf", "tanhf", "powf", "expf", "logf", "log10f", "sqrtf",
    "floorf", "ceilf", "fmodf", "frexpf", "ldexpf", "scalbnf", "expm1f",
    "atanf", "atan2f", "asinf", "acosf", "fabsf", "copysignf", "finitef",
    "nanf", "__ieee754", "__kernel", "__math", "__errno", "matherr",
)
LIBC_PREFIXES = (
    "__libc", "_init", "__aeabi", "memcpy", "memset", "memmove", "strlen",
    "abort", "malloc", "free", "_exit", "__cxa", "_ZdlPv", "_Znwj", "__gnu",
    "register_tm", "deregister",
)

MIN_SIG_WORDS = 8       # below this a fingerprint is not discriminating
MAX_ROUTINES_PER_WORD = 2  # a word in >2 routines is not distinctive
MATCH_THRESHOLD = 0.80  # fraction of fingerprint words that must be present


def classify_symbol(name: str) -> str:
    if name.startswith(LIBM_PREFIXES):
        return "libm"
    if name.startswith(LIBC_PREFIXES):
        return "libc_runtime"
    return "patch"


def elf_functions(elf: Path):
    """(addr, size, name) for every FUNC symbol, Thumb bit cleared."""
    out = subprocess.run(
        ["arm-none-eabi-readelf", "-sW", str(elf)],
        capture_output=True, text=True,
    ).stdout
    result = []
    for line in out.splitlines():
        m = re.match(
            r"\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+FUNC\s+\S+\s+\S+\s+\S+\s+(\S+)", line
        )
        if m:
            result.append((int(m.group(1), 16) & ~1, int(m.group(2)), m.group(3)))
    return result


def word_set(body: bytes, stride: int = 2) -> set[int]:
    return {
        struct.unpack_from("<I", body, i)[0]
        for i in range(0, len(body) - 3, stride)
    }


def build_signatures() -> dict[str, list[int]]:
    """Derive per-routine fingerprints from our own symbol-bearing builds."""
    per_routine: dict[str, list[set[int]]] = {}
    for elf in sorted((REPO / "build").glob("*.elf")):
        endl = REPO / "effects" / "builds" / f"{elf.stem}.endl"
        if not endl.exists():
            continue
        data = endl.read_bytes()
        for addr, size, name in elf_functions(elf):
            if size < 48:
                continue
            off = addr - DEFAULT_LOAD_ADDR
            body = data[off:off + size]
            if len(body) < size:
                continue
            per_routine.setdefault(name, []).append(word_set(body))

    # Keep only words present in every build's copy of the routine.
    sig = {
        name: set.intersection(*sets)
        for name, sets in per_routine.items()
        if len(sets) >= 3
    }
    # Drop words shared by many routines -- those identify nothing.
    counts: Counter = Counter()
    for words in sig.values():
        counts.update(words)
    sig = {
        name: {w for w in words if counts[w] <= MAX_ROUTINES_PER_WORD}
        for name, words in sig.items()
    }
    return {
        name: sorted(words)
        for name, words in sig.items()
        if len(words) >= MIN_SIG_WORDS
    }


def load_signatures(rebuild: bool = False) -> dict[str, set[int]]:
    if not rebuild and SIG_PATH.exists():
        raw = json.loads(SIG_PATH.read_text())
    else:
        raw = build_signatures()
        OUT_DIR.mkdir(parents=True, exist_ok=True)
        SIG_PATH.write_text(json.dumps(raw, indent=1))
    return {name: set(words) for name, words in raw.items()}


def detect_routines(data: bytes, sig: dict[str, set[int]]) -> dict[str, float]:
    """Match fingerprints against an image. Stride 1 catches every alignment."""
    present = word_set(data, stride=1)
    hits = {}
    for name, words in sig.items():
        score = len(words & present) / len(words)
        if score >= MATCH_THRESHOLD:
            hits[name] = round(score, 3)
    return hits


def instruction_mix(path: Path, load_addr: int, image_size: int) -> dict:
    """Coarse instruction census over the image body.

    This is a linear sweep, so literal pools and inline data decode as
    nonsense instructions. That makes absolute counts unreliable, but every
    image gets identical treatment, so the *ratios* are comparable across the
    corpus -- which is all they are used for.
    """
    start = load_addr + HEADER_SIZE
    out = subprocess.run(
        [
            "arm-none-eabi-objdump", "-D", "-b", "binary", "-m", "arm",
            "-M", "force-thumb", f"--adjust-vma={load_addr:#x}",
            f"--start-address={start:#x}",
            f"--stop-address={load_addr + HEADER_SIZE + image_size:#x}",
            str(path),
        ],
        capture_output=True, text=True,
    ).stdout

    total = fpu = calls = mem = 0
    for line in out.splitlines():
        parts = line.split("\t")
        if len(parts) < 3 or not parts[0].strip().rstrip(":").strip():
            continue
        mnem = parts[2].strip().split()[0] if parts[2].strip() else ""
        if not mnem:
            continue
        total += 1
        # VFP/NEON mnemonics all start with 'v' on ARM.
        if mnem.startswith("v"):
            fpu += 1
        if mnem in ("bl", "blx"):
            calls += 1
        if mnem.startswith(("ldr", "str")):
            mem += 1
    return {
        "decoded_instructions": total,
        "fpu_instructions": fpu,
        "fpu_ratio": round(fpu / total, 4) if total else 0.0,
        "call_instructions": calls,
        "memory_instructions": mem,
    }


def source_breakdown(name: str) -> dict | None:
    """For our own builds only: exact libm / runtime / effect byte split."""
    elf = REPO / "build" / f"{name}.elf"
    if not elf.exists():
        return None
    totals = {"libm": 0, "libc_runtime": 0, "patch": 0}
    seen = set()
    for addr, size, sym in elf_functions(elf):
        if size == 0 or addr in seen:
            continue
        seen.add(addr)
        totals[classify_symbol(sym)] += size
    total = sum(totals.values())
    if total:
        totals["libm_pct"] = round(100 * totals["libm"] / total, 1)
        totals["patch_pct"] = round(100 * totals["patch"] / total, 1)
    return totals


def analyze(path: Path, group: str, sig: dict[str, set[int]]) -> dict:
    data = path.read_bytes()
    hdr = parse_header(data, str(path))
    problems = validate(hdr)
    load_addr = hdr["load_addr"] or DEFAULT_LOAD_ADDR
    entries = classify_entries(path, hdr)

    detected = detect_routines(data, sig)
    # patch_init comes from internal/patch_main.c and is shared by every image
    # built against this ABI, so it confirms the method works but says nothing
    # about the maths a patch uses.
    libm_detected = sorted(n for n in detected if classify_symbol(n) == "libm")

    record = {
        "name": path.stem,
        "group": group,
        "path": str(path.relative_to(REPO)),
        "file_size": hdr["file_size"],
        "image_size": hdr["image_size"],
        "bss_size": hdr["bss_size"],
        "abi_version": hdr["abi_version"],
        "problems": problems,
        "libm_routines_detected": libm_detected,
        "all_routines_detected": sorted(detected),
        "param_name_inlined_stub": entries["agent_get_param_name"].get("inlined_stub"),
        "param_unit_inlined_stub": entries["agent_get_param_unit"].get("inlined_stub"),
        "entry_kinds": {k: v["kind"] for k, v in entries.items()},
    }
    record.update(instruction_mix(path, load_addr, hdr["image_size"]))
    breakdown = source_breakdown(path.stem) if group == "this_repo" else None
    if breakdown:
        record["symbol_breakdown"] = breakdown
    return record


def render_markdown(records: list[dict]) -> str:
    lines = ["# `.endl` corpus analysis", ""]
    lines.append(f"{len(records)} images analyzed.")
    lines.append("")
    for group, _ in GROUPS:
        rows = [r for r in records if r["group"] == group]
        if not rows:
            continue
        lines.append(f"## {group} ({len(rows)})")
        lines.append("")
        lines.append("| patch | image | bss | fpu ratio | libm routines | param name |")
        lines.append("|---|---|---|---|---|---|")
        for r in sorted(rows, key=lambda x: x["name"]):
            libm = ", ".join(r["libm_routines_detected"]) or "none detected"
            stub = "inlined stub" if r["param_name_inlined_stub"] else "forwards"
            lines.append(
                f"| {r['name']} | {r['image_size']} | {r['bss_size']} | "
                f"{r['fpu_ratio']:.3f} | {libm} | {stub} |"
            )
        lines.append("")
    return "\n".join(lines)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("paths", nargs="*", type=Path)
    ap.add_argument("--rebuild-signatures", action="store_true")
    ap.add_argument("--json-only", action="store_true")
    args = ap.parse_args(argv)

    sig = load_signatures(rebuild=args.rebuild_signatures)
    print(f"{len(sig)} routine fingerprints available", file=sys.stderr)

    targets = []
    if args.paths:
        for p in args.paths:
            targets.append((p, "adhoc"))
    else:
        for group, directory in GROUPS:
            for p in sorted(directory.rglob("*.endl")):
                targets.append((p, group))

    records = [analyze(p, g, sig) for p, g in targets]

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "summary.json").write_text(json.dumps(records, indent=2))
    (OUT_DIR / "summary.md").write_text(render_markdown(records))
    if not args.json_only:
        print(render_markdown(records))
    print(f"wrote {OUT_DIR/'summary.json'} and {OUT_DIR/'summary.md'}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
