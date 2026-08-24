# Recovering DSP from compiled `.endl` patches

**Date:** 2026-08-23
**Purpose:** non-commercial interoperability and study research — understand what
the platform's own patches do, and feed that back into this repo's authoring
conventions.

**Boundary:** what is recovered here informs our *conventions and documentation*.
No recovered code, coefficient, or routine is transcribed into `effects/`.
Polyend owns the factory plates
([`playground/polyend_plates/README.md`](../../playground/polyend_plates/README.md));
this is the same line the repo already holds on the `sthompsonjr` fork.

---

## Method

A `.endl` is a raw image with a self-describing 136-byte header
([`docs/endl-binary-format.md`](../endl-binary-format.md)), so getting to the
audio code takes three steps.

**1. Header → ABI entry points.** The header's 13 pointers are absolute, Thumb-bit
set. `scripts/endl_inspect.py` reads them straight out of the file.

**2. ABI entry → the real method.** Every `agent_*` entry is a thunk. It fetches
the singleton, loads the object's vtable, and tail-calls one slot:

```
bl    <singleton accessor>   ; guarded local-static
ldr   r1, [r0, #0]           ; vtable pointer
ldr   r4, [r1, #8]           ; slot +8
bx    ip                     ; tail-call
```

**3. Find the vtable.** The patch object lives in BSS, so its vtable *pointer* is
written at construction and is not in the file — but the vtable itself is, and it
is recognisable as a run of consecutive words that are all valid Thumb code
pointers into the image. `scripts/endl_inspect.py --vtable` scans for it.

Slot labels are read out of each binary's **own thunks**, never assumed from
`source/Patch.h`. That matters: Polyend's patch class has **12 virtuals to our
9**, and its layout differs — their `get_param_name` dispatches to `+24` where
ours is at `+16`, and they expose `getParamMin`/`Max`/`Default` as three separate
virtuals where we have one `getParameterMetadata` returning a struct. Applying
our slot order to their vtable would mislabel almost everything.

### Validation

The whole chain is checked against ground truth on our own builds, where
`arm-none-eabi-nm` knows the answer. For `chorus.endl` every recovered slot
matches:

| slot | recovered | `nm` |
|---|---|---|
| +0 | init | `PatchImpl::init()` |
| +4 | setWorkingBuffer | `PatchImpl::setWorkingBuffer(...)` |
| +8 | processAudio | `PatchImpl::processAudio(...)` |
| +12 | getParameterMetadata | `PatchImpl::getParameterMetadata(int)` |
| +16 | getParameterName | `PatchImpl::getParameterName(int)` |
| +20 | getParameterUnit | `Patch::getParameterUnit(int)` — inherited, not overridden |
| +24 | setParamValue | `PatchImpl::setParamValue(int, float)` |
| +28 | handleAction | `PatchImpl::handleAction(int)` |
| +32 | getStateLedColor | `PatchImpl::getStateLedColor()` |

Slot +20 resolving to the *base-class* `Patch::getParameterUnit` — the one we
deliberately leave unoverridden — is a good sign the recovery is reading real
structure rather than pattern-matching.

## Recovered entry points

| patch | image | vtable | `processAudio` |
|---|---|---|---|
| `Malleus_Fuzz` | 3,012 | `0x80000bd0` | `0x80000319` |
| `Slapper_Morton` | 3,036 | `0x80000c34` | `0x80000621` |
| `Yield_reverse_delay` | 3,948 | `0x80000f14` | `0x80000165` |
| `CloudStretch` (Playground) | 4,528 | `0x80001208` | `0x80000745` |
| `Wax` | 12,984 | `0x8000329c` | `0x8000291d` |

## What was and was not recovered

**Established for all five:** entry points, vtable layout, class shape (virtual
count and ordering), and the instruction-level census in
[`factory-patch-idioms.md`](factory-patch-idioms.md).

**Read in detail:** `Malleus_Fuzz` and `Yield_reverse_delay` — small enough to
follow end to end, and directly comparable to effects we ship.

**Not done:** complete signal-flow reconstruction of `Wax`, `Slapper_Morton` or
`CloudStretch`, and no coefficient extraction from any of them. The idiom-level
findings are what change how we write patches; recovering someone's exact filter
tuning would not, and is the part this repo has no business copying anyway.

## Tooling

```bash
python3 scripts/endl_inspect.py --entries --vtable <file.endl>
python3 scripts/endl_analyze.py
```

## See also

- [`factory-patch-idioms.md`](factory-patch-idioms.md) — the findings
- [`docs/endl-binary-format.md`](../endl-binary-format.md) — format spec
- [`docs/endl-corpus-study.md`](../endl-corpus-study.md) — corpus statistics
