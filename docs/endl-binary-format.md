# The `.endl` Binary Format

**Date:** 2026-08-23
**Status:** empirically verified across all 42 `.endl` files in this repo —
5 Polyend factory plates, 23 Playground-generated community patches, and this
repo's own 14 builds. Every field below was read out of real binaries and
cross-checked against `arm-none-eabi-nm` symbol output for the 14 where we have
ground truth.

Nothing in `docs/` previously recorded the header size or any field offset.
`docs/endless-reference.md` §4 named `PATCH_MAGIC` and `PATCH_ABI_VERSION` but
stopped there.

---

## 1. What an `.endl` actually is

A raw flat binary. `Makefile:71` is the whole of it:

```make
$(PATCH_BIN): $(PATCH_ELF) | $(BUILD_DIR)
	$(OBJCOPY) -O binary $< $@
```

There is no wrapper, no container, no compression, no checksum, and no
signature. Confirmed directly:

```
arm-none-eabi-objcopy -O binary build/chorus.elf /tmp/repro.bin
cmp effects/builds/chorus.endl /tmp/repro.bin   # identical
```

The `PTCH` header is **not** added by the build tooling. It is the
`PatchHeader` struct from [`internal/PatchABI.h`](../internal/PatchABI.h),
compiled into a `.patch_header` section that
[`internal/patch_imx.ld`](../internal/patch_imx.ld) pins at offset 0:

```ld
.patch_header ORIGIN(RAM) : ALIGN(4) { KEEP(*(.patch_header)) }
```

`internal/patch_main.c:53-77` materializes it. The linker script's
`ENTRY(patch_header)` names the *data* header, not a function — the firmware
never branches to offset 0, it reads a struct from there.

## 2. Header layout

136 bytes at offset 0. All fields little-endian.

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 4 | `magic` | `0x48435450` = `'PTCH'` |
| 4 | 2 | `abi_version` | `0x000B` in every binary observed |
| 6 | 2 | `flags` | `PATCH_FLAG_NONE`; 0 in all 42 |
| 8 | 4 | `init` | `void init(const PatchEnv*)` |
| 12 | 4 | `agent_update_buffers` | the audio callback |
| 16 | 4 | `agent_set_buffer` | receives the 9.6 MB working buffer |
| 20 | 4 | `agent_get_buffer_size` | |
| 24 | 4 | `agent_get_param_min` | |
| 28 | 4 | `agent_get_param_max` | |
| 32 | 4 | `agent_get_param_default` | |
| 36 | 4 | `agent_is_param_enabled` | `(idx, sourceId)`; 0 = knob, 1 = expression |
| 40 | 4 | `agent_get_param_name` | writes a string into a caller buffer |
| 44 | 4 | `agent_get_param_unit` | writes a string into a caller buffer |
| 48 | 4 | `agent_set_param` | |
| 52 | 4 | `agent_special_action` | footswitch press/hold |
| 56 | 4 | `agent_get_state_idx` | LED color, crosses as `int` |
| 60 | 4 | `image_size` | **excludes** the header |
| 64 | 4 | `bss_begin` | absolute address |
| 68 | 4 | `bss_size` | zeroed by the loader, not present in the file |
| 72 | 64 | `reserved[16]` | zero in all 42 |

All 13 pointers are **absolute addresses with the Thumb bit set** (`sym | 1`),
produced by the linker script:

```ld
patch_init_addr = patch_init | 1;      /* set Thumb bit */
```

The header comment in `PatchABI.h` says every agent pointer "may be NULL". No
binary in this corpus actually leaves one NULL.

## 3. Invariants

Two hold across all 42 files with no exceptions:

- `magic == 'PTCH'` and `abi_version == 0x000B`
- `136 + image_size == filesize`

The second is the useful one: it makes truncation detectable. The repo's
previous structural check read only the first 4 bytes
(`xxd -p -l 4`), so a truncated image with an
intact magic passed every gate the repo had. `scripts/endl_inspect.py --check`
now validates the whole header and is wired into that gate. (It also removes a
dependency on `xxd`, which is not installed in every environment this repo
runs in.)

`bss_size` is *not* covered by that identity — BSS is `NOLOAD`, reserved and
zeroed by the firmware after the image is copied in.

## 4. Load address

Not stored in the header. The image is linked to an absolute address supplied
at link time:

```make
PATCH_LOAD_ADDR ?= 0x80000000
LDFLAGS = ... -Wl,--defsym=PATCH_LOAD_ADDR=$(PATCH_LOAD_ADDR)
```

`internal/patch_imx.ld` places a single 512 KB `rxw` region there covering
code, rodata, data and bss together. `scripts/endl_inspect.py` recovers the
base from the entry pointers rather than assuming it.

The independent Rust SDK [`ceejbot/endless-rs`](https://github.com/ceejbot/endless-rs)
links with the same `patch_imx.ld` and the same `PATCH_LOAD_ADDR=0x80000000`,
which is useful corroboration that this is a firmware requirement rather than
an arbitrary default. The linker-script filename (`patch_imx.ld`) is the
strongest available hint at an NXP i.MX RT part; **no repo doc names an MCU
part number and this one does not either.**

## 5. The image is not position-independent

`CLAUDE.md` listed "position-independent code" as a hard constraint. The build
does not produce it:

```
$ arm-none-eabi-readelf -h build/chorus.elf
  Type:                              EXEC (Executable file)
  Entry point address:               0x80000000
$ arm-none-eabi-readelf -r build/chorus.elf
There are no relocations in this file.
$ arm-none-eabi-readelf -d build/chorus.elf
There is no dynamic section in this file.
```

No GOT symbol, and no `-fPIC` / `-fpie` / ROPI / RWPI flag anywhere in the
Makefile or linker script. The image is **absolutely bound** to
`PATCH_LOAD_ADDR` and would not run correctly if loaded anywhere else. Corrected
in `CLAUDE.md`, `docs/cycle-budget.md` and
`docs/fork-comparisons/sthompsonjr-wdf.md`.

Practically this matters little for patch authors — you cannot relocate a patch
anyway — but "relocatable image dropped at a fixed address" was describing a
mechanism the toolchain does not implement.

## 6. Cross-corpus measurements

| Group | n | `image_size` min / median / max | `bss_size` values |
|---|---|---|---|
| Polyend factory plates | 5 | 3,012 / 3,948 / 12,984 | 4, 112, 308 |
| Playground (community) | 23 | 2,632 / 5,764 / 10,044 | 4, 112, 128, 140, 308, 1,192, 3,556 |
| This repo (handcrafted) | 14 | 5,772 / 9,908 / 13,600 | 5 (every patch) |

Two things stand out, both pursued in
[`docs/endl-corpus-study.md`](endl-corpus-study.md):

- **Our images are the largest of the three groups by median**, roughly 2.5x
  Polyend's own factory plates.
- **Our `bss_size` is 5 bytes for every single patch.** Factory and Playground
  patches keep real state in BSS; we put essentially everything in the 9.6 MB
  working buffer, which `docs/patch-authoring-best-practices.md` §6 infers is
  external RAM. The recurring values 4 / 112 / 308 across *both* third-party
  groups suggest the factory plates and the Playground compiler share a build
  harness.

## 7. Tooling

- [`scripts/endl_inspect.py`](../scripts/endl_inspect.py) — parse, validate,
  and classify entry points in any `.endl`, with no ELF required.
  `--check` is the build gate; `--entries` labels each ABI slot;
  `--json` for machine consumption.
- [`scripts/endl_analyze.py`](../scripts/endl_analyze.py) — corpus-wide static
  analysis of the audio path.

Validation of the inspector itself: for all 14 builds where symbols exist, every
one of the 13 resolved entry addresses matches `arm-none-eabi-nm` exactly.

## See also

- [`internal/PatchABI.h`](../internal/PatchABI.h) — the authoritative struct
- [`internal/patch_imx.ld`](../internal/patch_imx.ld) — section placement
- [`internal/patch_main.c`](../internal/patch_main.c) — header materialization
- [`docs/endl-corpus-study.md`](endl-corpus-study.md) — what the corpus implies
  for patch authoring
- [`docs/endless-reference.md`](endless-reference.md) §4 — the prior, thinner
  ABI description
