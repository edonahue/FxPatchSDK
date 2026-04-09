#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="$ROOT_DIR/playground/polyend_plates"
FORCE=0
DRY_RUN=0
MANIFEST=0
USER_AGENT="Mozilla/5.0"
CURL_ARGS=(-fsSL -A "$USER_AGENT" --retry 5 --retry-delay 1 --retry-all-errors)

usage() {
    cat <<'EOF'
Usage: bash scripts/sync_polyend_plates.sh [options]

Sync Polyend Endless plate binaries from https://polyend.com/plates/ into a local
archive directory. This script is intended for local archival/reference syncing.

Options:
  --dest <dir>   Download destination. Default: playground/polyend_plates
  --force        Re-download files even if they already exist
  --dry-run      Print what would be downloaded without writing files
  --manifest     Print a tab-separated manifest: slug, product_url, filename
  -h, --help     Show this help text
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dest)
            [[ $# -ge 2 ]] || { echo "error: --dest requires a path" >&2; exit 1; }
            if [[ "$2" = /* ]]; then
                DEST_DIR="$2"
            else
                DEST_DIR="$ROOT_DIR/$2"
            fi
            shift 2
            ;;
        --force)
            FORCE=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --manifest)
            MANIFEST=1
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

mkdir -p "$DEST_DIR"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CATALOG_HTML="$TMP_DIR/plates.html"
curl "${CURL_ARGS[@]}" "https://polyend.com/plates/" -o "$CATALOG_HTML"

# The plates landing page links directly to each product page; use those URLs as the
# source of truth rather than guessing slugs or scraping tile text separately.
mapfile -t PRODUCT_URLS < <(
    rg -o 'https://polyend.com/product/[^" ]+/' "$CATALOG_HTML" | sort -u
)

if [[ ${#PRODUCT_URLS[@]} -eq 0 ]]; then
    echo "error: no Polyend plate product URLs found" >&2
    exit 1
fi

downloaded=0
skipped=0
missing=0
failed=0

if [[ "$MANIFEST" -eq 1 ]]; then
    printf 'slug\tproduct_url\tfilename\n'
fi

for product_url in "${PRODUCT_URLS[@]}"; do
    slug="${product_url#https://polyend.com/product/}"
    slug="${slug%/}"
    page_html="$TMP_DIR/$slug.html"

    if ! curl "${CURL_ARGS[@]}" "$product_url" -o "$page_html"; then
        echo "warning: failed to fetch product page: $product_url" >&2
        failed=$((failed + 1))
        continue
    fi

    # Product pages expose a direct "Download free effect" link to the hosted .endl
    # asset. Restrict the extraction to .endl URLs so we do not confuse them with demo
    # MP3 links or store assets on the same page.
    download_url="$(
        perl -0ne 'print "$1\n" if /href="([^"]+\.endl)"/s' "$page_html" | head -n 1
    )"

    if [[ -z "$download_url" ]]; then
        echo "warning: missing .endl download link: $product_url" >&2
        missing=$((missing + 1))
        continue
    fi

    filename="${download_url##*/}"
    target="$DEST_DIR/$filename"

    if [[ "$MANIFEST" -eq 1 ]]; then
        printf '%s\t%s\t%s\n' "$slug" "$product_url" "$filename"
    fi

    if [[ "$DRY_RUN" -eq 1 ]]; then
        if [[ -f "$target" && "$FORCE" -ne 1 ]]; then
            echo "dry-run: skip existing $filename" >&2
        else
            echo "dry-run: download $filename <- $download_url" >&2
        fi
        continue
    fi

    if [[ -f "$target" && "$FORCE" -ne 1 ]]; then
        skipped=$((skipped + 1))
        continue
    fi

    tmp_file="$TMP_DIR/$filename.part"
    if ! curl "${CURL_ARGS[@]}" "$download_url" -o "$tmp_file"; then
        echo "warning: failed to download $download_url" >&2
        failed=$((failed + 1))
        continue
    fi

    mv "$tmp_file" "$target"
    downloaded=$((downloaded + 1))
done

echo "Polyend Plates sync summary:"
echo "  products found: ${#PRODUCT_URLS[@]}"
echo "  downloaded:     $downloaded"
echo "  skipped:        $skipped"
echo "  missing links:  $missing"
echo "  failed:         $failed"

if [[ "$failed" -gt 0 ]]; then
    exit 1
fi
