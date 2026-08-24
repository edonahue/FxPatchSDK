#!/usr/bin/env python3
"""check_stack_usage.py — summarize -fstack-usage output after an ARM build.

Makefile:17's COMMON_FLAGS already passes -fstack-usage, so every compile
emits a .su sidecar (Makefile:57-63's build/%.o rules put it next to the
.o), but nothing has ever read them. Format, confirmed by direct
inspection of a real .su file:

    file:line:col:function-signature<TAB>bytes<TAB>static|dynamic|bound

This script globs build/**/*.su after a tests/check_arm_build.sh /
scripts/build_effects.sh run, aggregates per-effect total stack usage (the
sum across that effect's own generated_effects/<name>_PatchImpl.su -- the
shared internal/PatchCppWrapper.cpp and internal/patch_main.c wrapper code
is reported separately, since it isn't attributable to any one effect),
and flags:

  (a) any single function over a per-function threshold (a starting point,
      not a validated hard limit -- see FUNCTION_WARN_BYTES below)
  (b) any "dynamic" or "bound" qualifier at all. The no-heap/no-VLA
      constraints enforced elsewhere in this repo mean every function
      should report "static" stack usage; a dynamic/bound qualifier would
      indicate a real, unexpected variable-stack-size pattern.

Informational only, for now: this repo's "measure, don't assume"
convention means no hard-fail threshold has been validated against real
numbers yet, so this script always exits 0. Wire results into judgment,
not automated failure, until enough real .su data exists to pick one.

Usage:
    python3 scripts/check_stack_usage.py [build_dir]

build_dir defaults to build/ (matching build_effects.sh's own output
location).
"""
from __future__ import annotations

import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_BUILD_DIR = REPO_ROOT / "build"

# Starting point, not a validated limit -- tune once real numbers from a
# broader effect catalog exist. 512 bytes is a reasonable per-function
# budget for embedded audio code on a machine with an 8-16 KB stack.
FUNCTION_WARN_BYTES = 512

# Matches build/build/generated_effects/<name>_PatchImpl.su (see
# scripts/build_effects.sh's GEN_DIR and the Makefile's build/%.o rule,
# which doubles the "build/" prefix because PATCH_IMPL_SRC is already
# build/generated_effects/<name>_PatchImpl.cpp).
EFFECT_SU_RE = re.compile(r"generated_effects/(?P<name>.+)_PatchImpl\.su$")


class StackEntry:
    __slots__ = ("location", "signature", "bytes_", "qualifier")

    def __init__(self, location: str, signature: str, bytes_: int, qualifier: str) -> None:
        self.location = location
        self.signature = signature
        self.bytes_ = bytes_
        self.qualifier = qualifier


def parse_su_file(su_path: pathlib.Path) -> list[StackEntry]:
    entries = []
    for line in su_path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        # "file:line:col:signature<TAB>bytes<TAB>qualifier". The signature
        # itself commonly contains colons (e.g. "Patch::Color"), so only
        # the first three colon-separated fields (file, line, col) are
        # location; everything after that is the signature.
        location_and_signature, bytes_str, qualifier = line.rsplit("\t", 2)
        parts = location_and_signature.split(":", 3)
        if len(parts) == 4:
            location = ":".join(parts[:3])
            signature = parts[3]
        else:
            location = location_and_signature
            signature = ""
        entries.append(StackEntry(location, signature, int(bytes_str), qualifier))
    return entries


def find_su_files(build_dir: pathlib.Path) -> list[pathlib.Path]:
    return sorted(build_dir.rglob("*.su"))


def main() -> int:
    build_dir = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_BUILD_DIR

    su_files = find_su_files(build_dir)
    if not su_files:
        print(f"No .su files found under {build_dir} -- run a build first.", file=sys.stderr)
        return 0

    per_effect: dict[str, list[StackEntry]] = {}
    shared: list[tuple[str, StackEntry]] = []

    for su_path in su_files:
        entries = parse_su_file(su_path)
        match = EFFECT_SU_RE.search(su_path.as_posix())
        if match:
            per_effect.setdefault(match.group("name"), []).extend(entries)
        else:
            shared.extend((su_path.name, entry) for entry in entries)

    print("=== Stack Usage Summary (-fstack-usage) ===")
    print(f"Per-function warn threshold: {FUNCTION_WARN_BYTES} bytes (informational, not validated)")
    print("")

    any_dynamic = False
    warned_functions = 0

    for name in sorted(per_effect):
        entries = per_effect[name]
        total = sum(e.bytes_ for e in entries)
        over = [e for e in entries if e.bytes_ > FUNCTION_WARN_BYTES]
        dynamic = [e for e in entries if e.qualifier != "static"]
        warned_functions += len(over)
        if dynamic:
            any_dynamic = True

        flag_text = ""
        if dynamic:
            flag_text += f" [DYNAMIC/BOUND STACK: {len(dynamic)}]"
        if over:
            flag_text += f" [{len(over)} function(s) over {FUNCTION_WARN_BYTES}B]"

        print(f"{name:28} total={total:6} bytes across {len(entries):2} functions{flag_text}")
        for entry in over:
            print(f"    WARN  {entry.bytes_:5}B  {entry.signature}  ({entry.location})")
        for entry in dynamic:
            print(f"    DYNAMIC/BOUND  {entry.bytes_:5}B  {entry.signature}  ({entry.location})  qualifier={entry.qualifier}")

    if shared:
        print("")
        print("--- Shared wrapper code (not attributable to one effect) ---")
        for su_name, entry in shared:
            flag = ""
            if entry.qualifier != "static":
                flag = f"  [DYNAMIC/BOUND qualifier={entry.qualifier}]"
                any_dynamic = True
            elif entry.bytes_ > FUNCTION_WARN_BYTES:
                flag = f"  [WARN: over {FUNCTION_WARN_BYTES}B]"
                warned_functions += 1
            print(f"{su_name:32} {entry.bytes_:5}B  {entry.signature}{flag}")

    print("")
    print("=== Summary ===")
    print(f"Effects analyzed: {len(per_effect)}")
    print(f"Functions over {FUNCTION_WARN_BYTES}B: {warned_functions}")
    print(f"Dynamic/bound stack functions: {'YES -- investigate' if any_dynamic else 'none (expected, given no heap/VLA)'}")
    print("")
    print("Informational only -- no threshold here is a validated hard limit yet.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
