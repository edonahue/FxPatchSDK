#!/usr/bin/env bash
# analyze_effects.sh — Compile and probe all custom effects with host-side metrics.
#
# Usage:
#   bash tests/analyze_effects.sh
#
# Outputs:
#   build/effect_analysis/summary.json
#   build/effect_analysis/summary.md

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 is required"
    exit 1
fi

if ! command -v g++ >/dev/null 2>&1; then
    echo "error: g++ is required"
    exit 1
fi

python3 scripts/analyze_effects.py "$@"
