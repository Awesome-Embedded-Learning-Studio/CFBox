#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# gzip/gunzip round-trip
actual=$(echo "hello world" | "$CFBOX" gzip | "$CFBOX" gunzip)
if [[ "$actual" == "hello world" ]]; then ((++pass)); else echo "FAIL [gzip round-trip]: got $(printf '%q' "$actual")"; ((++fail)); fi

# gzip file round-trip
tmpdir=$(mktemp -d)
echo "test data" > "$tmpdir/input.txt"
"$CFBOX" gzip "$tmpdir/input.txt"
[[ -f "$tmpdir/input.txt.gz" ]] || { echo "FAIL: .gz not created"; ((++fail)); }
"$CFBOX" gunzip "$tmpdir/input.txt.gz"
actual=$(cat "$tmpdir/input.txt")
if [[ "$actual" == "test data" ]]; then ((++pass)); else echo "FAIL [gzip file]: got $(printf '%q' "$actual")"; ((++fail)); fi
rm -rf "$tmpdir"

echo "gzip: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
