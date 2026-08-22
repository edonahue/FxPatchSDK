#!/usr/bin/env bash
# run_pluginval.sh — build one effect's VST3 and validate it with pluginval.
#
# Usage:
#   bash scripts/run_pluginval.sh <effect_name> [strictness_level]
#
#   effect_name        basename of a file in effects/ (e.g. "tube_screamer")
#   strictness_level    1-10, default 8 (matches docs/vst-host-plan.md's
#                       verification section)
#
# Builds via the same manual sequence vst/README.md documents
# (cmake -S vst -B vst/build -DFX_EFFECT=<name> + cmake --build vst/build),
# locates the resulting .vst3 bundle, and runs pluginval against it,
# propagating pluginval's exit code.
#
# Requires pluginval on PATH: https://github.com/Tracktion/pluginval/releases

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if [[ $# -lt 1 ]]; then
    echo "usage: bash scripts/run_pluginval.sh <effect_name> [strictness_level]" >&2
    exit 1
fi

EFFECT_NAME="$1"
STRICTNESS="${2:-8}"

EFFECT_SRC="effects/${EFFECT_NAME}.cpp"
if [[ ! -f "$EFFECT_SRC" ]]; then
    echo "error: effect not found: $EFFECT_SRC" >&2
    exit 1
fi

if ! command -v pluginval >/dev/null 2>&1; then
    cat >&2 <<'EOF'
error: pluginval not found on PATH.

Install it from https://github.com/Tracktion/pluginval/releases (a
prebuilt binary is available for Linux/macOS/Windows -- no build required),
then make sure the extracted binary is on PATH.
EOF
    exit 1
fi

echo "=== Building ${EFFECT_NAME} (VST3) ==="
cmake -S vst -B vst/build -DFX_EFFECT="$EFFECT_NAME" -DCMAKE_BUILD_TYPE=Release
cmake --build vst/build --target FxPatchVST_VST3

# JUCE's exact artefact directory layout has shifted across versions (single-
# vs multi-config generator, Debug/Release subdirectory or not), so locate
# the bundle by name instead of hardcoding a path.
VST3_PATH="$(find vst/build -type d -iname "FxPatch_${EFFECT_NAME}.vst3" -print -quit)"
if [[ -z "$VST3_PATH" ]]; then
    echo "error: could not find a built .vst3 bundle for '${EFFECT_NAME}' under vst/build" >&2
    echo "       (looked for a directory named FxPatch_${EFFECT_NAME}.vst3)" >&2
    exit 1
fi

echo ""
echo "=== Running pluginval (strictness ${STRICTNESS}) on ${VST3_PATH} ==="

# pluginval opens a real plugin editor as part of its test suite, which
# segfaults with no display server available (confirmed in this repo's own
# headless sandbox). Wrap with a virtual display via xvfb-run when DISPLAY
# isn't set and xvfb-run is available; run directly otherwise (a real
# desktop session, or a CI runner that already provides its own display).
if [[ -z "${DISPLAY:-}" ]] && command -v xvfb-run >/dev/null 2>&1; then
    echo "(no DISPLAY set -- running under xvfb-run)"
    xvfb-run -a pluginval --strictness-level "$STRICTNESS" --validate "$VST3_PATH"
else
    pluginval --strictness-level "$STRICTNESS" --validate "$VST3_PATH"
fi
