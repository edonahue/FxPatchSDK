#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

TOOLCHAIN_PREFIX="${TOOLCHAIN:-arm-none-eabi-}"

# internal/patch_imx.ld fixes a single 512 KiB (524288-byte) RAM region for
# the entire patch image -- .text+.rodata+.data+.bss all count against it.
# arm-none-eabi-size's "dec" column is exactly that sum.
readonly RAM_BUDGET_BYTES=524288
readonly RAM_WARN_BYTES=419430  # 80% of budget

echo "=== Polyend Endless ARM Build Check ==="

for tool in "${TOOLCHAIN_PREFIX}g++" "${TOOLCHAIN_PREFIX}objcopy" "${TOOLCHAIN_PREFIX}size"; do
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
over_budget=0
near_budget=0

for effect_path in "${PATCH_SOURCES[@]}"; do
    effect_name="$(basename "$effect_path" .cpp)"
    output_bin="effects/builds/${effect_name}.endl"
    output_elf="build/${effect_name}.elf"
    expected=$((expected + 1))

    if [[ ! -f "$output_bin" ]]; then
        echo "MISSING: $output_bin"
        missing=$((missing + 1))
        continue
    fi

    # Validate the whole 136-byte PatchHeader, not just the 4-byte magic: the
    # firmware reads image_size, bss_begin/bss_size and 13 Thumb entry pointers
    # out of it, so a file with a good magic can still be unloadable. This also
    # drops the dependency on xxd, which is not installed everywhere.
    if ! python3 scripts/endl_inspect.py --check "$output_bin"; then
        echo "BAD HEADER: $output_bin"
        bad_header=$((bad_header + 1))
        continue
    fi

    size_bytes="$(stat -c '%s' "$output_bin")"

    # internal/patch_imx.ld's single 512 KiB RAM region covers .text+.rodata+
    # .data+.bss together -- the .elf's "dec" column from arm-none-eabi-size
    # is that sum, and the more meaningful figure to gate on than the raw
    # .endl file size.
    ram_bytes="$("${TOOLCHAIN_PREFIX}size" "$output_elf" | tail -1 | awk '{print $4}')"
    ram_pct=$((ram_bytes * 100 / RAM_BUDGET_BYTES))

    if [[ "$ram_bytes" -gt "$RAM_BUDGET_BYTES" ]]; then
        echo "OVER BUDGET: $output_bin (${size_bytes} bytes on disk, ${ram_bytes}/${RAM_BUDGET_BYTES} RAM bytes, ${ram_pct}%)"
        over_budget=$((over_budget + 1))
    elif [[ "$ram_bytes" -gt "$RAM_WARN_BYTES" ]]; then
        echo "WARN: $output_bin (${size_bytes} bytes on disk, ${ram_bytes}/${RAM_BUDGET_BYTES} RAM bytes, ${ram_pct}%)"
        near_budget=$((near_budget + 1))
    else
        echo "PASS: $output_bin (${size_bytes} bytes on disk, ${ram_bytes}/${RAM_BUDGET_BYTES} RAM bytes, ${ram_pct}%)"
    fi
done

echo ""
echo "=== Summary ==="
echo "Expected outputs: $expected"
echo "Missing outputs:  $missing"
echo "Bad headers:      $bad_header"
echo "Near RAM budget (>80%): $near_budget"
echo "Over RAM budget:        $over_budget"

if [[ "$missing" -gt 0 || "$bad_header" -gt 0 || "$over_budget" -gt 0 ]]; then
    exit 1
fi

# Informational only (see scripts/check_stack_usage.py's own header) -- no
# threshold there is validated enough to fail the build on yet.
if command -v python3 >/dev/null 2>&1; then
    echo ""
    python3 "$REPO_ROOT/scripts/check_stack_usage.py"
fi

echo ""
echo "All effects built successfully."
