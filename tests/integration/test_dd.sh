#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

echo -n "hello world" > "$tmpdir/in"

# basic file copy
"$CFBOX" dd if="$tmpdir/in" of="$tmpdir/out" bs=1 2>/dev/null
if [[ "$(cat "$tmpdir/out")" == "hello world" ]]; then ((++pass)); else echo "FAIL [basic]"; ((++fail)); fi

# count limits bytes
"$CFBOX" dd if="$tmpdir/in" of="$tmpdir/out2" bs=1 count=5 2>/dev/null
if [[ "$(cat "$tmpdir/out2")" == "hello" ]]; then ((++pass)); else echo "FAIL [count]"; ((++fail)); fi

# stdin -> stdout (bs=1 count=5)
actual=$(echo -n "stdin data" | "$CFBOX" dd bs=1 count=5 2>/dev/null)
if [[ "$actual" == "stdin" ]]; then ((++pass)); else echo "FAIL [stdin]: '$actual'"; ((++fail)); fi

# bs size suffix (1K)
head -c 2048 /dev/zero | tr '\0' 'x' > "$tmpdir/big"
"$CFBOX" dd if="$tmpdir/big" of="$tmpdir/bigout" bs=1K 2>/dev/null
if [[ $(wc -c < "$tmpdir/bigout") -eq 2048 ]]; then ((++pass)); else echo "FAIL [bs suffix]"; ((++fail)); fi

# conv=notrunc keeps the tail
echo -n "XXXXXXXXXX" > "$tmpdir/nt"
echo -n "YYY" > "$tmpdir/y"
"$CFBOX" dd if="$tmpdir/y" of="$tmpdir/nt" bs=1 count=3 conv=notrunc 2>/dev/null
if [[ "$(cat "$tmpdir/nt")" == "YYYXXXXXXX" ]]; then ((++pass)); else echo "FAIL [notrunc]"; ((++fail)); fi

# skip
echo -n "ABCDEFGHIJ" > "$tmpdir/abc"
"$CFBOX" dd if="$tmpdir/abc" of="$tmpdir/abco" bs=1 skip=2 count=3 2>/dev/null
if [[ "$(cat "$tmpdir/abco")" == "CDE" ]]; then ((++pass)); else echo "FAIL [skip]"; ((++fail)); fi

# status=none suppresses the transfer stats on stderr
actual=$(echo -n "x" | "$CFBOX" dd bs=1 status=none 2>&1 >/dev/null)
if [[ -z "$actual" ]]; then ((++pass)); else echo "FAIL [status=none]: '$actual'"; ((++fail)); fi

# default prints stats to stderr
actual=$(echo -n "x" | "$CFBOX" dd bs=1 2>&1 >/dev/null)
if [[ "$actual" == *"records out"* ]]; then ((++pass)); else echo "FAIL [default stats]"; ((++fail)); fi

echo "dd: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
