#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

OUT_DIR="effects/builds"
GEN_DIR="build/generated_effects"
TOOLCHAIN="${TOOLCHAIN:-arm-none-eabi-}"
CLEAN_FIRST=0
ONE_EFFECT=""

usage() {
    cat <<'EOF'
Usage: bash scripts/build_effects.sh [options]

Build one or all top-level effects/*.cpp patches into local .endl artifacts.

Options:
  --effect <name>  Build only effects/<name>.cpp
  --clean          Remove build/ and effects/builds/*.endl before building
  -h, --help       Show this help text
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --effect)
            [[ $# -ge 2 ]] || { echo "error: --effect requires a name" >&2; exit 1; }
            ONE_EFFECT="$2"
            shift 2
            ;;
        --clean)
            CLEAN_FIRST=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

mkdir -p "$OUT_DIR" "$GEN_DIR"

if [[ "$CLEAN_FIRST" -eq 1 ]]; then
    rm -rf build
    mkdir -p "$OUT_DIR" "$GEN_DIR"
    find "$OUT_DIR" -maxdepth 1 -type f -name '*.endl' -delete
fi

shopt -s nullglob
EFFECT_FILES=(effects/*.cpp)

if [[ -n "$ONE_EFFECT" ]]; then
    target="effects/${ONE_EFFECT}.cpp"
    if [[ ! -f "$target" ]]; then
        echo "error: effect not found: $target" >&2
        exit 1
    fi
    EFFECT_FILES=("$target")
fi

if [[ ${#EFFECT_FILES[@]} -eq 0 ]]; then
    echo "error: no top-level effects/*.cpp files found" >&2
    exit 1
fi

PASS=0
FAIL=0

for effect_path in "${EFFECT_FILES[@]}"; do
    effect_name="$(basename "$effect_path" .cpp)"
    generated_src="$GEN_DIR/${effect_name}_PatchImpl.cpp"
    output_bin="$OUT_DIR/${effect_name}.endl"

    # Effects include ../source/Patch.h when built standalone. Repoint the generated
    # build copy at Patch.h so the stock SDK Makefile can compile it as PatchImpl.cpp.
    sed 's#"../source/Patch.h"#"Patch.h"#' "$effect_path" > "$generated_src"

    if make \
        TOOLCHAIN="$TOOLCHAIN" \
        PATCH_NAME="$effect_name" \
        PATCH_IMPL_SRC="$generated_src" \
        PATCH_BIN="$output_bin" \
        >/tmp/build_effects_"$effect_name".log 2>&1
    then
        echo "PASS: $effect_name -> $output_bin"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $effect_name"
        tail -n 20 /tmp/build_effects_"$effect_name".log | sed 's/^/  | /'
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Built: $PASS passed, $FAIL failed"

if [[ "$FAIL" -gt 0 ]]; then
    exit 1
fi
