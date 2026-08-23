#!/usr/bin/env python3
"""Parse and validate the PatchHeader at the start of a Polyend Endless .endl image.

An .endl file is a raw `objcopy -O binary` dump of the linked patch ELF. The
firmware loads it verbatim to PATCH_LOAD_ADDR and reads a PatchHeader from
offset 0, so every field in that header is present in the file itself -- no ELF,
no symbols, and no debug info are needed to inspect one.

The authoritative struct is `internal/PatchABI.h`; the layout constants below
mirror it. Field offsets were confirmed empirically against every .endl in the
repo (see docs/endl-binary-format.md).

Usage:
    python3 scripts/endl_inspect.py PATH...            # human-readable report
    python3 scripts/endl_inspect.py --check PATH...    # validate only, exit 1 on failure
    python3 scripts/endl_inspect.py --json PATH...     # machine-readable
    python3 scripts/endl_inspect.py --entries PATH...  # also classify entry points

`--check` is what tests/build_effects.sh uses as its structural gate. It
validates the whole header rather than only the 4-byte magic.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

# --- constants mirrored from internal/PatchABI.h -----------------------------

PATCH_MAGIC = 0x48435450  # 'PTCH'
PATCH_ABI_VERSION = 0x000B
HEADER_SIZE = 136
DEFAULT_LOAD_ADDR = 0x80000000  # Makefile's PATCH_LOAD_ADDR default

# Header field order == on-disk order. Offsets are derived, not hardcoded, so
# this list stays the single source of truth for the pointer table.
ENTRY_NAMES = [
    "init",
    "agent_update_buffers",
    "agent_set_buffer",
    "agent_get_buffer_size",
    "agent_get_param_min",
    "agent_get_param_max",
    "agent_get_param_default",
    "agent_is_param_enabled",
    "agent_get_param_name",
    "agent_get_param_unit",
    "agent_set_param",
    "agent_special_action",
    "agent_get_state_idx",
]
ENTRY_TABLE_OFFSET = 8
OFF_IMAGE_SIZE = 60
OFF_BSS_BEGIN = 64
OFF_BSS_SIZE = 68
OFF_RESERVED = 72
RESERVED_WORDS = 16

# Instruction count before the first return, used only as a coarse size label.
# "minimal" does NOT mean unimplemented: agent_get_buffer_size legitimately
# compiles to `ldr r0, =2400000; bx lr` because it returns a constant. Use
# `param_string_stub` below for the one case where "does this do anything at
# all?" is actually decidable.
MINIMAL_MAX_INSNS = 6

# NOTE: an earlier version of this file tried to decide whether an entry was
# "really implemented" from its instruction shape. That heuristic was wrong in
# both directions -- it read Polyend's vtable-forwarding thunk as a string
# producer, and this SDK's own switch-based implementation as a stub -- so it
# was removed. Whether a patch supplies knob names is answered directly by
# scanning the image for name strings; see scripts/endl_analyze.py.


class HeaderError(Exception):
    """The bytes at offset 0 are not a usable PatchHeader."""


def parse_header(data: bytes, path: str = "<bytes>") -> dict:
    """Decode a PatchHeader. Raises HeaderError if the file is too short."""
    if len(data) < HEADER_SIZE:
        raise HeaderError(
            f"{path}: file is {len(data)} bytes, shorter than the "
            f"{HEADER_SIZE}-byte PatchHeader"
        )

    magic, abi_version, flags = struct.unpack_from("<IHH", data, 0)
    entries = {}
    for i, name in enumerate(ENTRY_NAMES):
        (value,) = struct.unpack_from("<I", data, ENTRY_TABLE_OFFSET + 4 * i)
        entries[name] = value
    (image_size,) = struct.unpack_from("<I", data, OFF_IMAGE_SIZE)
    (bss_begin,) = struct.unpack_from("<I", data, OFF_BSS_BEGIN)
    (bss_size,) = struct.unpack_from("<I", data, OFF_BSS_SIZE)
    reserved = list(
        struct.unpack_from("<%dI" % RESERVED_WORDS, data, OFF_RESERVED)
    )

    # The load address is not stored in the header -- the image is linked to an
    # absolute address, so recover it from the entry pointers instead. Every
    # pointer is `symbol | 1` (Thumb), and the header itself sits at the base,
    # so masking the low 16 bits of any real pointer recovers the base for the
    # 64 KiB-aligned load addresses this SDK uses.
    load_addr = None
    for value in entries.values():
        if value:
            load_addr = value & 0xFFFF0000
            break

    return {
        "path": path,
        "file_size": len(data),
        "magic": magic,
        "magic_ascii": struct.pack("<I", magic).decode("ascii", "replace"),
        "abi_version": abi_version,
        "flags": flags,
        "entries": entries,
        "image_size": image_size,
        "bss_begin": bss_begin,
        "bss_size": bss_size,
        "reserved": reserved,
        "load_addr": load_addr,
    }


def validate(hdr: dict) -> list[str]:
    """Return a list of problems; empty means the header is structurally sound."""
    problems = []
    if hdr["magic"] != PATCH_MAGIC:
        problems.append(
            f"bad magic {hdr['magic']:#010x} (expected {PATCH_MAGIC:#010x} 'PTCH')"
        )
    if hdr["abi_version"] != PATCH_ABI_VERSION:
        problems.append(
            f"ABI version {hdr['abi_version']:#06x} != "
            f"PATCH_ABI_VERSION {PATCH_ABI_VERSION:#06x}"
        )
    # image_size excludes the header (linker script: "exclude header from
    # image_size"), so header + image must account for the whole file.
    expected = HEADER_SIZE + hdr["image_size"]
    if expected != hdr["file_size"]:
        problems.append(
            f"size mismatch: header({HEADER_SIZE}) + image_size({hdr['image_size']})"
            f" = {expected}, but file is {hdr['file_size']} bytes"
        )
    if hdr["entries"]["init"] == 0:
        problems.append("init entry point is NULL -- firmware cannot start this patch")
    for name, value in hdr["entries"].items():
        if value and not value & 1:
            problems.append(
                f"{name} = {value:#010x} has no Thumb bit set; the firmware "
                "calls these as Thumb functions"
            )
    if hdr["bss_size"] and not hdr["bss_begin"]:
        problems.append(
            f"bss_size = {hdr['bss_size']} but bss_begin is NULL"
        )
    if any(hdr["reserved"]):
        problems.append("reserved[] is not all zero")
    return problems


def _disassemble(path: Path, addr: int, load_addr: int, nbytes: int, prefix: str):
    """Disassemble nbytes of Thumb code at an absolute address. Returns mnemonics."""
    start = addr & ~1
    out = subprocess.run(
        [
            f"{prefix}objdump", "-D", "-b", "binary", "-m", "arm",
            "-M", "force-thumb", f"--adjust-vma={load_addr:#x}",
            f"--start-address={start:#x}", f"--stop-address={start + nbytes:#x}",
            str(path),
        ],
        capture_output=True, text=True,
    )
    insns = []
    for line in out.stdout.splitlines():
        # objdump columns are tab-separated: "addr:\thex bytes\tmnemonic\toperands".
        # Mnemonic and operands are separate fields, so rejoin everything past
        # the raw bytes to get the full instruction text.
        parts = line.split("\t")
        if len(parts) >= 3 and parts[0].strip().rstrip(":").strip():
            insns.append(" ".join(p.strip() for p in parts[2:]).strip())
    return insns


def classify_entries(path: Path, hdr: dict, prefix: str = "arm-none-eabi-") -> dict:
    """Label each ABI entry implemented / stub / null by disassembling it.

    Reports the entry address and a coarse size label (instruction count to
    the first return). Size alone does not establish whether an entry does
    anything useful -- see the NOTE near the top of this file.
    """
    result = {}
    load_addr = hdr["load_addr"] or DEFAULT_LOAD_ADDR
    for name, addr in hdr["entries"].items():
        if not addr:
            result[name] = {"addr": 0, "kind": "null", "insns": 0}
            continue
        insns = _disassemble(path, addr, load_addr, 48, prefix)
        body = []
        for ins in insns:
            body.append(ins)
            # First return ends the function. `bx lr` and `pop {...,pc}` are
            # the two forms GCC emits for these small entry thunks.
            if ins.startswith("bx ") and "lr" in ins:
                break
            if ins.startswith("pop") and "pc" in ins:
                break
        count = len(body)
        kind = "minimal" if count <= MINIMAL_MAX_INSNS else "substantive"
        result[name] = {"addr": addr, "kind": kind, "insns": count}
    return result


# The C++ patch object lives in BSS, so its vtable pointer is written at
# construction time and is not in the file. The vtable itself is in the image
# though, and is recognisable: a run of consecutive words that are all valid
# Thumb code pointers into this image. The first such run is always the
# PatchHeader entry table at offset 8; the class vtable is a later one.
MIN_VTABLE_ENTRIES = 8

# Slot labels are NOT assumed from source/Patch.h's declaration order. Polyend's
# own SDK declares a different set (their plates have 12 virtuals to our 9, and
# their get_param_name dispatches to +24 where ours is at +16), so labelling a
# third-party vtable with our order would be wrong. Instead each ABI entry
# thunk is disassembled and the vtable offset it dispatches through is read out
# of it -- ground truth, per binary.
#
# A thunk looks like:  ldr rA, [rB, #0]   ; load vtable pointer from the object
#                      ldr rC, [rA, #N]   ; load slot N
THUNK_VTABLE_RE = re.compile(r"ldr\s+(r\d+), \[(r\d+), #(\d+)\]")


def find_vtables(data: bytes, load_addr: int = DEFAULT_LOAD_ADDR) -> list[dict]:
    """Locate candidate C++ vtables by scanning for runs of Thumb code pointers."""
    end = load_addr + len(data)
    words = [
        struct.unpack_from("<I", data, i)[0] for i in range(0, len(data) - 3, 4)
    ]

    def is_code_ptr(w: int) -> bool:
        return bool(w & 1) and load_addr + HEADER_SIZE <= (w & ~1) < end

    runs = []
    i = 0
    while i < len(words):
        if is_code_ptr(words[i]):
            j = i
            while j < len(words) and is_code_ptr(words[j]):
                j += 1
            if j - i >= MIN_VTABLE_ENTRIES:
                runs.append({
                    "addr": load_addr + i * 4,
                    "count": j - i,
                    "entries": words[i:j],
                    "is_patch_header": (i * 4) == ENTRY_TABLE_OFFSET,
                })
            i = j
        else:
            i += 1
    return runs


def thunk_vtable_offsets(path: Path, hdr: dict, prefix: str = "arm-none-eabi-") -> dict:
    """Map vtable byte-offset -> ABI entry name, by reading each entry's thunk.

    Each agent_* entry point is a thunk that fetches the object's vtable and
    tail-calls one slot. Reading the offset out of the thunk tells us what that
    slot is, without assuming anything about the patch class's layout.
    """
    load_addr = hdr["load_addr"] or DEFAULT_LOAD_ADDR
    found = {}
    for name, addr in hdr["entries"].items():
        if not addr:
            continue
        vtable_reg = None
        for ins in _disassemble(path, addr, load_addr, 64, prefix):
            m = THUNK_VTABLE_RE.match(ins)
            if not m:
                continue
            dst, src, off = m.group(1), m.group(2), int(m.group(3))
            if off == 0:
                vtable_reg = dst          # this loaded the vtable pointer
            elif src == vtable_reg:
                found.setdefault(off, name)
                break
    return found


def report(hdr: dict, entries: dict | None, problems: list[str]) -> str:
    lines = []
    lines.append(f"{hdr['path']}")
    lines.append(
        f"  magic          {hdr['magic']:#010x} ({hdr['magic_ascii']!r})"
    )
    lines.append(f"  abi_version    {hdr['abi_version']:#06x}")
    lines.append(f"  flags          {hdr['flags']:#06x}")
    lines.append(
        f"  image_size     {hdr['image_size']} "
        f"(+{HEADER_SIZE} header = {HEADER_SIZE + hdr['image_size']}, "
        f"file {hdr['file_size']})"
    )
    lines.append(f"  bss_begin      {hdr['bss_begin']:#010x}")
    lines.append(f"  bss_size       {hdr['bss_size']}")
    if hdr["load_addr"] is not None:
        lines.append(f"  load_addr      {hdr['load_addr']:#010x} (recovered)")
    lines.append("  entries:")
    for name, addr in hdr["entries"].items():
        suffix = ""
        if entries:
            info = entries[name]
            suffix = f"  [{info['kind']}, {info['insns']} insn]"
        shown = f"{addr:#010x}" if addr else "NULL"
        lines.append(f"    {name:<26} {shown}{suffix}")
    if problems:
        lines.append("  PROBLEMS:")
        lines.extend(f"    - {p}" for p in problems)
    else:
        lines.append("  OK")
    return "\n".join(lines)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("paths", nargs="+", type=Path)
    ap.add_argument("--check", action="store_true",
                    help="validate only; print nothing on success, exit 1 on failure")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    ap.add_argument("--entries", action="store_true",
                    help="disassemble each entry and report its size")
    ap.add_argument("--vtable", action="store_true",
                    help="locate the C++ vtable and name its slots")
    ap.add_argument("--prefix", default="arm-none-eabi-",
                    help="toolchain prefix for objdump (default: arm-none-eabi-)")
    args = ap.parse_args(argv)

    results = []
    failures = 0
    for path in args.paths:
        try:
            data = path.read_bytes()
            hdr = parse_header(data, str(path))
        except (OSError, HeaderError) as exc:
            failures += 1
            if args.json:
                results.append({"path": str(path), "error": str(exc)})
            else:
                print(f"FAIL {path}: {exc}", file=sys.stderr)
            continue

        problems = validate(hdr)
        entries = classify_entries(path, hdr, args.prefix) if args.entries else None
        vtables = (find_vtables(data, hdr["load_addr"] or DEFAULT_LOAD_ADDR)
                   if args.vtable else None)
        if problems:
            failures += 1

        if args.json:
            hdr["problems"] = problems
            if entries:
                hdr["entry_kinds"] = entries
            if vtables is not None:
                hdr["vtables"] = vtables
            results.append(hdr)
        elif args.check:
            if problems:
                print(f"FAIL {path}", file=sys.stderr)
                for p in problems:
                    print(f"  - {p}", file=sys.stderr)
        else:
            print(report(hdr, entries, problems))
            slot_names = thunk_vtable_offsets(path, hdr, args.prefix)
            for vt in vtables or []:
                if vt["is_patch_header"]:
                    continue
                print(f"  vtable @ {vt['addr']:#010x} ({vt['count']} slots):")
                for k, value in enumerate(vt["entries"]):
                    label = slot_names.get(k * 4, "")
                    print(f"    +{k * 4:<3} {value:#010x}  {label}")

    if args.json:
        print(json.dumps(results, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
