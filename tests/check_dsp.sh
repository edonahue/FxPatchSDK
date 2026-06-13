#!/usr/bin/env bash
# check_dsp.sh — Build and run per-primitive unit tests for source/dsp/.
#
# Companion to tests/check_patches.sh. Where check_patches.sh syntax-checks
# every effect translation unit, this script builds and runs each
# tests/dsp/<primitive>_test.cpp host-side and reports PASS/FAIL.
#
# Usage:
#   bash tests/check_dsp.sh          # from repository root
#
# Requirements:
#   - g++ with C++20 support (g++ 10 or newer)
#   - Run from repository root

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

FLAGS=(
    -std=c++20
    -O2
    -fsingle-precision-constant
    -Wall
    -Wextra
    -Wdouble-promotion
    -I source
)

BUILD_DIR="build/dsp_tests"
mkdir -p "$BUILD_DIR"

shopt -s nullglob
TEST_FILES=(tests/dsp/*_test.cpp)

if [[ ${#TEST_FILES[@]} -eq 0 ]]; then
    echo "No DSP tests found in tests/dsp/"
    exit 1
fi

echo "=== Polyend Endless DSP Primitive Tests ==="
echo "Compiler: $(g++ --version | head -1)"
echo "Flags: ${FLAGS[*]}"
echo ""

PASS=0
FAIL=0
FAIL_TESTS=()

for src in "${TEST_FILES[@]}"; do
    name=$(basename "$src" .cpp)
    bin="$BUILD_DIR/$name"
    if ! build_out=$(g++ "${FLAGS[@]}" "$src" -o "$bin" 2>&1); then
        echo "BUILD-FAIL: $src"
        echo "$build_out" | sed 's/^/  | /'
        FAIL=$((FAIL + 1))
        FAIL_TESTS+=("$name")
        continue
    fi
    if run_out=$("$bin" 2>&1); then
        echo "$run_out"
        PASS=$((PASS + 1))
    else
        echo "RUN-FAIL: $name"
        echo "$run_out" | sed 's/^/  | /'
        FAIL=$((FAIL + 1))
        FAIL_TESTS+=("$name")
    fi
done

echo ""
echo "=== Summary ==="
echo "DSP tests: $PASS passed, $FAIL failed"

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo "Failed tests:"
    printf '  %s\n' "${FAIL_TESTS[@]}"
    exit 1
fi

echo ""
echo "All DSP tests passed."
exit 0
