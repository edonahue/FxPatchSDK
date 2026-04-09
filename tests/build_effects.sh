#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

TOOLCHAIN_PREFIX="${TOOLCHAIN:-arm-none-eabi-}"

echo "=== Polyend Endless ARM Build Check ==="

for tool in "${TOOLCHAIN_PREFIX}g++" "${TOOLCHAIN_PREFIX}objcopy"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool" >&2
        exit 1
    fi
done

echo "Compiler: $(${TOOLCHAIN_PREFIX}g++ --version | head -1)"
echo "Objcopy:  $(${TOOLCHAIN_PREFIX}objcopy --version | head -1)"
echo ""

bash scripts/build_effects.sh --clean

shopt -s nullglob
PATCH_SOURCES=(effects/*.cpp)

expected=0
missing=0
bad_header=0

for effect_path in "${PATCH_SOURCES[@]}"; do
    effect_name="$(basename "$effect_path" .cpp)"
    output_bin="effects/builds/${effect_name}.endl"
    expected=$((expected + 1))

    if [[ ! -f "$output_bin" ]]; then
        echo "MISSING: $output_bin"
        missing=$((missing + 1))
        continue
    fi

    header="$(xxd -p -l 4 "$output_bin")"
    if [[ "$header" != "50544348" ]]; then
        echo "BAD HEADER: $output_bin"
        bad_header=$((bad_header + 1))
        continue
    fi

    size_bytes="$(stat -c '%s' "$output_bin")"
    echo "PASS: $output_bin (${size_bytes} bytes)"
done

echo ""
echo "=== Summary ==="
echo "Expected outputs: $expected"
echo "Missing outputs:  $missing"
echo "Bad headers:      $bad_header"

if [[ "$missing" -gt 0 || "$bad_header" -gt 0 ]]; then
    exit 1
fi

echo ""
echo "All effects built successfully."
