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

# The two string-returning entries are the only ABI slots where a do-nothing
# implementation is unambiguously detectable: the SDK's stub writes a single
# zero byte and returns (`movs r3,#0; strb r3,[r2]; bx lr`), whereas any real
# implementation must either call a helper or store more than one byte.
PARAM_STRING_ENTRIES = ("agent_get_param_name", "agent_get_param_unit")


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

    Reports a coarse size label plus, for the two string entries, a decisive
    `param_string_stub` flag. That flag is what distinguishes Polyend's factory
    plates (which implement get_param_name/unit) from images built by this SDK
    (which write only a terminator).
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
        info = {"addr": addr, "kind": kind, "insns": count}
        if name in PARAM_STRING_ENTRIES:
            stores = [i for i in body if i.split()[0].startswith("str")]
            calls = [i for i in body if i.split()[0] in ("bl", "blx")]
            info["param_string_stub"] = len(stores) <= 1 and not calls
        result[name] = info
    return result


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
            if info.get("param_string_stub") is True:
                suffix += "  STUB (writes only a terminator)"
            elif info.get("param_string_stub") is False:
                suffix += "  provides a string"
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
                    help="disassemble each entry and label stub vs implemented")
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
        if problems:
            failures += 1

        if args.json:
            hdr["problems"] = problems
            if entries:
                hdr["entry_kinds"] = entries
            results.append(hdr)
        elif args.check:
            if problems:
                print(f"FAIL {path}", file=sys.stderr)
                for p in problems:
                    print(f"  - {p}", file=sys.stderr)
        else:
            print(report(hdr, entries, problems))

    if args.json:
        print(json.dumps(results, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
