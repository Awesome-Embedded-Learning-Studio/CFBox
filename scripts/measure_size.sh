#!/usr/bin/env bash
set -euo pipefail

# measure_size.sh — Report binary section sizes for CFBox
# Usage: scripts/measure_size.sh [path-to-cfbox-binary]

binary="${1:-build/cfbox}"

if [[ ! -x "$binary" ]]; then
    echo "ERROR: $binary not found or not executable" >&2
    exit 1
fi

echo "=== Binary: $binary ==="
echo ""

# Raw file size
raw_size=$(wc -c < "$binary")
echo "Raw file size:    $(numfmt --to=iec "$raw_size")"

# Section breakdown
echo ""
echo "Section sizes (size):"
size "$binary"

# Dynamic dependencies
echo ""
echo "Dynamic dependencies:"
if ldd "$binary" 2>/dev/null; then
    echo "(dynamically linked)"
else
    echo "(statically linked)"
fi

# Top-10 largest symbols
echo ""
echo "Top 10 largest symbols:"
nm -S -C "$binary" 2>/dev/null | awk '$2 ~ /^[0-9a-f]+$/ { print $2, $3 }' \
    | sort -rn | head -10 | while read -r hex_size name; do
        size_dec=$((16#$hex_size))
        printf "%10s  %s\n" "$(numfmt --to=iec "$size_dec")" "$name"
    done
