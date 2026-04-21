#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "line1\nline2\nline3\n" > "$tmpdir/nl.txt"

# nl on a file produces line numbers
out=$("$CFBOX" nl "$tmpdir/nl.txt")
if [[ "$out" == *"1"*"line1"* ]] && [[ "$out" == *"2"*"line2"* ]] && [[ "$out" == *"3"*"line3"* ]]; then
    ((++pass))
else
    echo "FAIL [nl line numbers]: got $(printf '%q' "$out")"; ((++fail))
fi

# --help
assert_exit 0 nl --help
((++pass))

echo "nl: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
