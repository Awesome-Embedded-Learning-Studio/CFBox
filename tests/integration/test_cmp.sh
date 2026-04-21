#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# identical
echo "abc" > "$tmpdir/a.txt"
echo "abc" > "$tmpdir/b.txt"
"$CFBOX" cmp "$tmpdir/a.txt" "$tmpdir/b.txt" && rc=0 || rc=$?
if [[ $rc -eq 0 ]]; then ((++pass)); else echo "FAIL [cmp identical]: rc=$rc"; ((++fail)); fi

# different
echo "axc" > "$tmpdir/c.txt"
"$CFBOX" cmp "$tmpdir/a.txt" "$tmpdir/c.txt" > /dev/null 2>&1 && rc=0 || rc=$?
if [[ $rc -eq 1 ]]; then ((++pass)); else echo "FAIL [cmp different]: rc=$rc"; ((++fail)); fi

echo "cmp: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
