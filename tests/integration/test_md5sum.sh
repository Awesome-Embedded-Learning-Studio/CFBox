#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# md5sum of known file
printf "hello\n" > "$tmpdir/md5.txt"
out=$("$CFBOX" md5sum "$tmpdir/md5.txt")
if [[ "$out" == *"b1946ac92492d2347c6235b4d2611184"* ]]; then ((++pass)); else echo "FAIL [md5sum known]: got $(printf '%q' "$out")"; ((++fail)); fi

# md5sum of empty file = d41d8cd98f00b204e9800998ecf8427e
printf "" > "$tmpdir/empty.txt"
out=$("$CFBOX" md5sum "$tmpdir/empty.txt")
if [[ "$out" == *"d41d8cd98f00b204e9800998ecf8427e"* ]]; then ((++pass)); else echo "FAIL [md5sum empty]: got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 md5sum --help
((++pass))

echo "md5sum: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
