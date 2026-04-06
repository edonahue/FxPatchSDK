#!/usr/bin/env bash
# check_patches.sh — Syntax and lint validation for Polyend Endless SDK patches
#
# Runs a syntax-only compile check on all effects/*.cpp files using the host g++
# with flags that match the SDK build (minus ARM-specific flags not available on
# the host machine). This catches type errors, missing includes, and most common
# coding mistakes before deploying to hardware.
#
# Usage:
#   bash tests/check_patches.sh          # from repository root
#
# Requirements:
#   - g++ with C++20 support (g++ 10 or newer)
#   - Run from repository root (not from tests/)
#
# See tests/README.md for more details.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Flags matching the SDK build (ARM-specific flags omitted — host compiler only)
FLAGS=(
    -std=c++20
    -fno-exceptions
    -fno-rtti
    -fsingle-precision-constant
    -Wdouble-promotion
    -Wall
    -Wextra
    -fsyntax-only
    -I source
)

PASS=0
FAIL=0
FAIL_FILES=()

echo "=== Polyend Endless Patch Syntax Check ==="
echo "Compiler: $(g++ --version | head -1)"
echo "Flags: ${FLAGS[*]}"
echo ""

# Check each patch file in effects/
shopt -s nullglob
PATCH_FILES=(effects/*.cpp)

if [[ ${#PATCH_FILES[@]} -eq 0 ]]; then
    echo "No patch files found in effects/"
    exit 1
fi

for f in "${PATCH_FILES[@]}"; do
    # Capture stderr (compiler errors) while suppressing stdout
    if output=$(g++ "${FLAGS[@]}" "$f" 2>&1); then
        echo "PASS: $f"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $f"
        echo "$output" | sed 's/^/  | /'
        FAIL=$((FAIL + 1))
        FAIL_FILES+=("$f")
    fi
done

echo ""
echo "=== Lint Checks ==="

LINT_WARNINGS=0

# Check for heap allocation (forbidden by SDK: no new/malloc/std containers)
HEAP_MATCHES=$(grep -rn --include="*.cpp" -E '\bnew\s|\bmalloc\b|\bstd::vector\b|\bstd::string\b|\bstd::deque\b' effects/ 2>/dev/null || true)
if [[ -n "$HEAP_MATCHES" ]]; then
    echo "WARNING: Heap allocation found (forbidden in SDK — no new/malloc/std containers):"
    echo "$HEAP_MATCHES" | sed 's/^/  /'
    LINT_WARNINGS=$((LINT_WARNINGS + 1))
else
    echo "OK: No heap allocation detected"
fi

# Check for bare double-precision literals (should have 'f' suffix)
# Matches patterns like 3.14, 0.5, 1.0 without trailing 'f' or 'F'
# Excludes comment lines and #include lines
DOUBLE_MATCHES=$(grep -rn --include="*.cpp" -E '[^a-zA-Z_][0-9]+\.[0-9]+[^fFeEdD\s,);}\]>]' effects/ 2>/dev/null \
    | grep -v '^\s*//' \
    | grep -v '^[^:]*:#include' \
    || true)
if [[ -n "$DOUBLE_MATCHES" ]]; then
    echo "WARNING: Possible double-precision literals (add 'f' suffix):"
    echo "$DOUBLE_MATCHES" | sed 's/^/  /'
    LINT_WARNINGS=$((LINT_WARNINGS + 1))
else
    echo "OK: No obvious double-precision literal issues"
fi

# Check for hardcoded sample rate (should use kSampleRate constant)
SR_MATCHES=$(grep -rn --include="*.cpp" -E '\b48000\b' effects/ 2>/dev/null || true)
if [[ -n "$SR_MATCHES" ]]; then
    echo "WARNING: Hardcoded sample rate 48000 (use Patch::kSampleRate instead):"
    echo "$SR_MATCHES" | sed 's/^/  /'
    LINT_WARNINGS=$((LINT_WARNINGS + 1))
else
    echo "OK: No hardcoded sample rate"
fi

# Check that getInstance() is defined in each patch (required by PatchCppWrapper.cpp)
INSTANCE_MISSING=()
for f in "${PATCH_FILES[@]}"; do
    if ! grep -q 'getInstance' "$f"; then
        INSTANCE_MISSING+=("$f")
    fi
done
if [[ ${#INSTANCE_MISSING[@]} -gt 0 ]]; then
    echo "WARNING: Missing getInstance() in:"
    printf '  %s\n' "${INSTANCE_MISSING[@]}"
    LINT_WARNINGS=$((LINT_WARNINGS + 1))
else
    echo "OK: All patches define getInstance()"
fi

echo ""
echo "=== Summary ==="
echo "Compile: $PASS passed, $FAIL failed"
echo "Lint warnings: $LINT_WARNINGS"

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo "Failed files:"
    printf '  %s\n' "${FAIL_FILES[@]}"
    exit 1
fi

if [[ $LINT_WARNINGS -gt 0 ]]; then
    echo ""
    echo "Lint warnings found. Review above before deploying."
    # Exit 0 — lint warnings are informational, not blocking
fi

echo ""
echo "All patches passed syntax check."
exit 0
