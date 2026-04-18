#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <target_directory>"
    echo "Generate symlinks for all registered applets."
    exit 1
fi

target_dir="$1"
cfbox="${CFBOX:-$(dirname "$0")/../build/cfbox}"

if [[ ! -x "$cfbox" ]]; then
    echo "ERROR: cfbox not found at $cfbox"
    exit 1
fi

mkdir -p "$target_dir"

# Get applet list from cfbox --list
while IFS= read -r line; do
    name=$(echo "$line" | awk '{print $1}')
    [[ -z "$name" ]] && continue
    ln -sf "$cfbox" "$target_dir/$name"
    echo "  linked $name"
done < <("$cfbox" --list)

echo "Symlinks created in $target_dir"
