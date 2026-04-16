# Patch Validation Tests

This directory contains automated validation scripts for Polyend Endless SDK patches
stored in `effects/`. Treat these checks as a fast preflight before device testing, not
as proof that a patch is production-ready on the pedal.

---

## check_patches.sh

Performs two categories of checks on all `effects/*.cpp` files:

### 1. Syntax-Only Compile Check

Runs `g++ -fsyntax-only` with flags that mirror the SDK build, minus ARM-specific
options that are not available on the host machine. This catches:

- Missing or incorrect `#include` paths
- Type errors and implicit conversions
- Missing `override` specifiers
- Double-promotion warnings (`-Wdouble-promotion`)
- Most C++ syntax errors

**What it does NOT catch:**

- Linker errors (symbols not found at link time)
- ARM-specific ABI or alignment issues
- Runtime behavior (audio glitches, incorrect filter behavior)
- Bugs that only appear with specific input signals

For a real SDK build, use the dedicated ARM build script:

```bash
bash tests/build_effects.sh
```

### 2. Lint Checks

Grep-based pattern checks that warn about common SDK violations:

| Check | Rule |
|---|---|
| Heap allocation | No `new`, `malloc`, `std::vector`, `std::string` — SDK has no heap |
| Double literals | All floating-point literals must have `f` suffix (e.g., `3.14f`) |
| Hardcoded sample rate | Use `Patch::kSampleRate` instead of `48000` |
| Missing `getInstance()` | Every patch must define `Patch* Patch::getInstance()` |

Lint warnings are informational and do not cause the script to exit non-zero.

---

## Running the Tests

From the repository root:

```bash
bash tests/check_patches.sh
bash tests/build_effects.sh
bash tests/analyze_effects.sh
```

`check_patches.sh` stays fast and host-only.
`build_effects.sh` runs the real ARM toolchain across all top-level `effects/*.cpp`
files and verifies the generated `.endl` outputs in `effects/builds/`.
`analyze_effects.sh` runs the host-side probe harness against every top-level effect and
regenerates the synthetic summary artifacts under `build/effect_analysis/`.

Expected output when all patches pass:

```
=== Polyend Endless Patch Syntax Check ===
Compiler: g++ (Ubuntu 12.3.0-1ubuntu1~22.04) 12.3.0
Flags: -std=c++20 -fno-exceptions ...

PASS: effects/chorus.cpp
PASS: effects/bbe_sonic_stomp.cpp
PASS: effects/back_talk_reverse_delay.cpp
PASS: effects/big_muff.cpp
PASS: effects/harmonica.cpp
PASS: effects/klon_centaur.cpp
PASS: effects/mxr_distortion_plus.cpp
PASS: effects/phase_90.cpp
PASS: effects/tube_screamer.cpp
PASS: effects/wah.cpp

=== Lint Checks ===
OK: No heap allocation detected
OK: No obvious double-precision literal issues
OK: No hardcoded sample rate
OK: All patches define getInstance()

=== Summary ===
Compile: 10 passed, 0 failed
Lint warnings: 0

All patches passed syntax check.
```

---

## build_effects.sh

Builds all top-level `effects/*.cpp` files through the real ARM toolchain and checks
that each expected output exists at `effects/builds/<effect>.endl` with a valid `PTCH`
header.

This script verifies the repo's actual deployable build path, not just syntax.

Requirements beyond `check_patches.sh`:

- `arm-none-eabi-g++`
- `arm-none-eabi-objcopy`

Underlying build helper:

```bash
bash scripts/build_effects.sh
```

This helper rewrites each effect's `../source/Patch.h` include into a generated local
`PatchImpl.cpp` under `build/generated_effects/`, then invokes `make` with
`PATCH_IMPL_SRC=...` so every top-level effect can be compiled without mutating
`source/PatchImpl.cpp`.

---

## analyze_effects.sh

Builds and runs the host-side probe harness for every top-level `effects/*.cpp` patch.

Outputs:

- `build/effect_analysis/summary.json`
- `build/effect_analysis/summary.md`

This script is useful for:

- comparing before/after patch behavior during retunes
- checking bypass, hold-mode, and full knob sweeps with repeatable synthetic signals
- catching “mostly louder” versus “actually more nonlinear” behavior in drive patches
- checking whether a `Level` / `Output` control reaches unity too early or spends most
  of its travel leaning on the digital ceiling

The current summaries include:

- ordinary RMS plus a guitar-midband RMS proxy
- unity-position detection for the designated level/output control
- `peak_abs`, `hot_sample_ratio`, and `clip_sample_ratio` so limiter abuse shows up in the report

It is still not a substitute for hardware listening.

---

## Adding New Checks

To add a new lint rule, append to the lint section of `check_patches.sh`:

```bash
# Example: warn if printf is used (not available on Endless hardware)
PRINTF_MATCHES=$(grep -rn 'printf\|fprintf\|sprintf' effects/ 2>/dev/null || true)
if [[ -n "$PRINTF_MATCHES" ]]; then
    echo "WARNING: printf family not available on Endless hardware:"
    echo "$PRINTF_MATCHES" | sed 's/^/  /'
    ((LINT_WARNINGS++))
else
    echo "OK: No printf usage"
fi
```

---

## Requirements

- `g++` with C++20 support (GCC 10+ or Clang 12+)
- `arm-none-eabi-g++` and `arm-none-eabi-objcopy` for `tests/build_effects.sh`
- Run from the repository root (not from `tests/`)
- The `source/Patch.h` header must be present at `source/Patch.h`

---

## Known Limitations

1. **Host syntax is still not hardware validation:** `check_patches.sh` uses host `g++`
   and cannot replace a real ARM build. Use `tests/build_effects.sh` for deployable
   artifact verification.

2. **No audio testing:** Neither script runs the patch with real audio. Functional
   correctness must still be validated by ear on hardware.

   `tests/analyze_effects.sh` adds synthetic-signal probing, which is useful for
   comparative measurements but still does not replace hardware listening.

   For `effects/bbe_sonic_stomp.cpp`, hardware validation should explicitly include the
   expression pedal because this repo's current SDK wrapper routes expression to `param 2`
   and the patch uses
   `param 2` as `Midrange`.

   For `effects/mxr_distortion_plus.cpp`, hardware validation should explicitly check the
   full Distortion sweep, the audibility of the Tone control, and that expression-driven
   `Level` reaches near-unity around noon before delivering obvious boost above noon.

   For `effects/back_talk_reverse_delay.cpp`, hardware validation should explicitly check
   chunk-edge smoothness at extreme `Speed` settings, the climb of `Repetitions` into
   near-runaway behavior, the feel of expression-driven `Mix`, and the contrast between
   normal and texture modes.

   For `effects/big_muff.cpp`, hardware validation should explicitly check the full
   `Sustain` sweep, the midpoint and extremes of the Muff-style `Tone` control, the
   loudness behavior of expression-driven `Blend`, and the contrast between the core
   Ram's Head voice and the hold-toggle Tone Bypass alternate mode.

   For `effects/tube_screamer.cpp`, hardware validation should explicitly check the full
   `Drive` sweep, that `Level` reaches near-unity around noon without collapsing into a
   ceiling-limited shelf, the feel of expression-driven `Tone`, and the contrast between
   the TS808 default voice and the hold-toggle TS9 alternate voice.

   For `effects/klon_centaur.cpp`, hardware validation should explicitly check the low-
   to-mid `Gain` range for clean/dirty openness, the usefulness of the active `Treble`
   shelf, that `Output` reaches near-unity around noon with real boost above it, and the
   contrast between the stock voice and the hold-toggle Tone Mod alternate.

   For `effects/phase_90.cpp`, hardware validation should explicitly check the full
   center-knob `Speed` sweep, the usefulness of expression-mirrored speed, the stronger
   block-logo feel versus the smoother script-mode feel, and the absence of volume jumps
   or pops when toggling the hold voice.

   For `effects/harmonica.cpp`, hardware validation should explicitly check the
   heel-to-toe expression sweep (should read as a hand-cup morph, not a synthy filter),
   that the `Reed` knob delivers a continuous reed-to-bullet-amp breakup without a
   sudden level jump, the audible contrast between the Open and Cupped voicings, and
   the absence of pops on bypass re-engage or voicing toggle. Because the SDK has no
   pitch shifter, also verify the illusion holds for single-note blues phrasing on the
   neck pickup and breaks (as expected) on open low strings and chords.

3. **Double-literal detection is heuristic:** The grep pattern for double literals may
   produce false positives in comments or string literals. Review warnings manually.

4. **Excludes `effects/examples/`:** The automated scripts intentionally target only
   top-level `effects/*.cpp`, not `effects/examples/*.cpp`. To check examples manually, run:
   ```bash
   g++ -std=c++20 -fno-exceptions -fno-rtti -fsingle-precision-constant \
       -Wdouble-promotion -fsyntax-only -I source effects/examples/reverb.cpp
   ```
