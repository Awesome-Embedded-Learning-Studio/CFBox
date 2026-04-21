#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# identical files
echo "hello" > "$tmpdir/a.txt"
echo "hello" > "$tmpdir/b.txt"
set +e
"$CFBOX" diff "$tmpdir/a.txt" "$tmpdir/b.txt" > /dev/null 2>&1
rc=$?
set -e
if [[ $rc -eq 0 ]]; then ((++pass)); else echo "FAIL [diff identical]: rc=$rc"; ((++fail)); fi

# different files
echo "world" > "$tmpdir/c.txt"
set +e
"$CFBOX" diff "$tmpdir/a.txt" "$tmpdir/c.txt" > /dev/null 2>&1
rc=$?
set -e
if [[ $rc -eq 1 ]]; then ((++pass)); else echo "FAIL [diff different]: rc=$rc"; ((++fail)); fi

# unified format has --- and +++ markers
actual=$("$CFBOX" diff -u "$tmpdir/a.txt" "$tmpdir/c.txt" 2>&1 || true)
if echo "$actual" | grep -q "^---" && echo "$actual" | grep -q "^+++"; then
    ((++pass))
else
    echo "FAIL [diff unified]: $actual"; ((++fail))
fi

# diff | patch round-trip
echo -e "line1\nline2\nline3" > "$tmpdir/orig.txt"
echo -e "line1\nchanged\nline3" > "$tmpdir/modified.txt"
set +e
"$CFBOX" diff "$tmpdir/orig.txt" "$tmpdir/modified.txt" | "$CFBOX" patch "$tmpdir/orig.txt"
set -e
actual=$(cat "$tmpdir/orig.txt")
expected=$(cat "$tmpdir/modified.txt")
if [[ "$actual" == "$expected" ]]; then ((++pass)); else echo "FAIL [diff|patch]: got $(printf '%q' "$actual")"; ((++fail)); fi

echo "diff: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
