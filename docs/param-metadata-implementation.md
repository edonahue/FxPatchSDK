# Parameter Names and Units

**Date:** 2026-08-23
**Motivation:** [`docs/endl-corpus-study.md`](endl-corpus-study.md), Finding 2.
**Status:** implemented and verified at the binary level; **not verified on
hardware.**

---

## Why

The firmware ABI reserves two slots for per-parameter display text —
`agent_get_param_name` and `agent_get_param_unit` in `PatchHeader`
([`internal/PatchABI.h`](../internal/PatchABI.h)). The public SDK implemented
both as a single `'\0'` write, so no patch built against it could supply a knob
name however it was written.

Binary analysis of the other 28 patches in the repo found that every Polyend
factory plate and 22 of 23 Playground-generated community patches carry knob-name
strings — `SPEED`, `DRIFT`, `DelayMs`, `WahRange`, `TremRate`, `Feedbck`. All 14
of ours carried none. Polyend's own wrapper tail-calls through a vtable slot,
so their internal SDK exposes these as virtuals on the patch class; the public
one does not.

## What changed

**[`source/Patch.h`](../source/Patch.h)** — two new virtuals returning
`const char*`, defaulting to `nullptr`:

```cpp
virtual const char* getParameterName(int paramIdx) { (void)paramIdx; return nullptr; }
virtual const char* getParameterUnit(int paramIdx) { (void)paramIdx; return nullptr; }
```

Returning a pointer to a string literal costs nothing and needs no storage, which
matters given the no-heap constraint. Both have default implementations, so every
existing patch — and any third-party patch written against the old header —
still compiles untouched.

**[`internal/PatchCppWrapper.cpp`](../internal/PatchCppWrapper.cpp)** — both
entry points now forward to the virtuals through a bounded copy:

```cpp
static void copyBounded(char* out, size_t bufferSize, const char* text);
```

It truncates rather than overruns, always terminates, and treats `nullptr` as an
empty string. This also fixes a latent bug: the old `get_param_unit` wrote
`out[0]` without checking `bufferSize`, unlike its `get_param_name` sibling. The
parameter index is now range-checked against `endless::kParams` as well; the
numeric accessors around it still do not check, which is unchanged behavior.

**All 14 effects** implement `getParameterName`, sourced from each patch's own
header-comment control table. Names are kept to 8 characters or fewer — the
firmware's buffer size is documented nowhere, and `copyBounded` truncates
silently, so short is safe.

| Effect | Left | Mid | Right / Exp |
|---|---|---|---|
| back_talk_reverse_delay | Speed | Repeats | Mix |
| bbe_sonic_stomp | Contour | Process | Midrange |
| big_muff / big_muff_wdf | Sustain | Tone | Blend |
| chorus | Rate | Depth | Mix |
| dimension_chorus | Rate | Depth | Width |
| funk_machine_envelope_filter | Sens | Reso | Bias |
| harmonica | Tone | Reed | Waa |
| klon_centaur | Gain | Treble | Output |
| mxr_distortion_plus | Dist | Tone | Level |
| phase_90 | *(unused)* | Speed | Speed |
| tube_screamer | Drive | Level | Tone |
| tube_screamer_wdf | Drive | Tone | Level |
| wah | Mix | Q | Wah |

`phase_90`'s left knob returns `nullptr` because it is deliberately unused, which
is exactly what the null return is for.

## Units are deliberately left empty

Every parameter in this SDK is a normalized `0..1` control with a patch-defined,
usually nonlinear taper. There is no truthful unit string for "0.42 of a drive
taper", and emitting `%` would imply the firmware displays a percentage of
something meaningful. `getParameterUnit` therefore exists, is wired through, and
returns `nullptr` everywhere.

Polyend evidently takes the other view in at least one case — `Slapper_Morton`
carries `DelayMs` — but they know their own display formatting and we do not.
Revisit once someone can see what the pedal actually renders.

## Verification

- `bash tests/check_patches.sh` — 14/14 lint PASS under `-Werror`, 9/9 DSP tests.
- `bash tests/build_effects.sh` — 14/14 ARM builds, structural gate PASS, 0
  functions over the stack threshold.
- **DSP unchanged:** `bash tests/analyze_effects.sh` compared field-by-field
  against the pre-change run — **12,107 scalar fields, 0 differences.** Adding
  metadata must not alter audio, and it did not.
- **Binary-level confirmation:** rebuilt images now contain the name strings
  (`Rate`, `Depth`, `Mix`, `Treble`, `Output`, …) where before they contained
  none.

### Cost

Between +280 and +316 bytes of image per effect, mean +308 — the string literals
plus the switch:

| effect | before | after | delta |
|---|---|---|---|
| `back_talk_reverse_delay` | 8568 | 8876 | +308 |
| `bbe_sonic_stomp` | 13420 | 13736 | +316 |
| `big_muff` | 9752 | 10064 | +312 |
| `big_muff_wdf` | 11796 | 12108 | +312 |
| `chorus` | 7896 | 8204 | +308 |
| `dimension_chorus` | 9732 | 10044 | +312 |
| `funk_machine_envelope_filter` | 10380 | 10692 | +312 |
| `harmonica` | 10480 | 10788 | +308 |
| `klon_centaur` | 5596 | 5908 | +312 |
| `mxr_distortion_plus` | 5676 | 5988 | +312 |
| `phase_90` | 7816 | 8096 | +280 |
| `tube_screamer` | 9460 | 9772 | +312 |
| `tube_screamer_wdf` | 10124 | 10436 | +312 |
| `wah` | 8608 | 8908 | +300 |
 The largest effect is now 13,736 bytes, **2.62%** of the 512 KB
region. Irrelevant against that budget.

## What is not verified

Nobody here has watched an Endless display a knob name. That the ABI defines the
slot, that Polyend's SDK wires it to a virtual, and that every Polyend-authored
patch fills it is strong evidence the firmware consumes it — but it is inference,
not observation. If the pedal turns out to ignore these, the cost of having been
wrong is ~308 bytes per patch and no behavioral change.

## See also

- [`docs/endl-corpus-study.md`](endl-corpus-study.md) — the finding that
  motivated this
- [`docs/endl-binary-format.md`](endl-binary-format.md) — header layout
- [`internal/PatchABI.h`](../internal/PatchABI.h) — the ABI definition
