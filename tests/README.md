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

For a real SDK build, use the ARM cross-compiler and the repo Makefile:

```bash
cp effects/mxr_distortion_plus.cpp source/PatchImpl.cpp
# Edit include to "Patch.h" (not "../source/Patch.h")
make TOOLCHAIN=/path/to/arm-none-eabi-
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
```

Expected output when all patches pass:

```
=== Polyend Endless Patch Syntax Check ===
Compiler: g++ (Ubuntu 12.3.0-1ubuntu1~22.04) 12.3.0
Flags: -std=c++20 -fno-exceptions ...

PASS: effects/chorus.cpp
PASS: effects/mxr_distortion_plus.cpp

=== Lint Checks ===
OK: No heap allocation detected
OK: No obvious double-precision literal issues
OK: No hardcoded sample rate
OK: All patches define getInstance()

=== Summary ===
Compile: 2 passed, 0 failed
Lint warnings: 0

All patches passed syntax check.
```

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
- Run from the repository root (not from `tests/`)
- The `source/Patch.h` header must be present at `source/Patch.h`

---

## Known Limitations

1. **No ARM artifact is produced:** Host `g++` cannot produce the deployed `.endl`
   image. A patch that passes this check may still fail to link or behave incorrectly
   on hardware if it uses unsupported calls or relies on undefined ARM ABI behavior.

2. **No audio testing:** The script does not run the patch with real audio. Functional
   correctness must still be validated by ear on hardware.

3. **Double-literal detection is heuristic:** The grep pattern for double literals may
   produce false positives in comments or string literals. Review warnings manually.

4. **Excludes `effects/examples/`:** Only `effects/*.cpp` is checked, not
   `effects/examples/*.cpp`. To check examples manually, run:
   ```bash
   g++ -std=c++20 -fno-exceptions -fno-rtti -fsingle-precision-constant \
       -Wdouble-promotion -fsyntax-only -I source effects/examples/reverb.cpp
   ```
