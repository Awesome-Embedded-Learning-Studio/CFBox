#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "hello world\n" > "$tmpdir/cksum.txt"

# cksum on a file produces numbers (checksum and size)
out=$("$CFBOX" cksum "$tmpdir/cksum.txt")
if [[ "$out" =~ ^[0-9]+[[:space:]]+[0-9]+ ]]; then ((++pass)); else echo "FAIL [cksum output]: expected numbers, got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 cksum --help
((++pass))

echo "cksum: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
