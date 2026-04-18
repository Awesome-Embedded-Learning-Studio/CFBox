#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "hello world\nfoo bar baz\n" > "$tmpdir/wc.txt"

run_test() {
    local name="$1"
    shift
    local actual
    actual=$("$CFBOX" wc "$@")
    echo "$actual"
}

# default: lines words bytes
actual=$("$CFBOX" wc "$tmpdir/wc.txt")
if [[ "$actual" == *"2"*"5"* ]]; then ((++pass)); else echo "FAIL [wc default] got $(printf '%q' "$actual")"; ((++fail)); fi

# -l
actual=$("$CFBOX" wc -l "$tmpdir/wc.txt")
if [[ "$actual" == *"2"* ]]; then ((++pass)); else echo "FAIL [wc -l]"; ((++fail)); fi

# -w
actual=$("$CFBOX" wc -w "$tmpdir/wc.txt")
if [[ "$actual" == *"5"* ]]; then ((++pass)); else echo "FAIL [wc -w]"; ((++fail)); fi

# -c: "hello world\nfoo bar baz\n" = 24 bytes
actual=$("$CFBOX" wc -c "$tmpdir/wc.txt")
if [[ "$actual" == *"24"* ]]; then ((++pass)); else echo "FAIL [wc -c] got $(printf '%q' "$actual")"; ((++fail)); fi

# stdin
actual=$(printf 'hello world\n' | "$CFBOX" wc)
if [[ "$actual" == *"1"*"2"* ]]; then ((++pass)); else echo "FAIL [wc stdin] got $(printf '%q' "$actual")"; ((++fail)); fi

echo "wc: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
