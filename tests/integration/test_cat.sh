#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

echo -n "hello world" > "$tmpdir/file1.txt"
printf "line1\nline2\nline3\n" > "$tmpdir/file2.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" cat "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

run_test "basic" "hello world" "$tmpdir/file1.txt"

# -n numbering
actual=$("$CFBOX" cat -n "$tmpdir/file1.txt")
if [[ "$actual" == *"1"*"hello world"* ]]; then ((++pass)); else echo "FAIL [cat -n]"; ((++fail)); fi

# -b non-empty numbering
actual=$("$CFBOX" cat -b "$tmpdir/file2.txt")
if [[ "$actual" == *"1"*"line1"* ]]; then ((++pass)); else echo "FAIL [cat -b]"; ((++fail)); fi

# multi file
actual=$("$CFBOX" cat "$tmpdir/file1.txt" "$tmpdir/file2.txt")
if [[ "$actual" == *"hello world"*"line1"* ]]; then ((++pass)); else echo "FAIL [cat multi]"; ((++fail)); fi

# stdin
actual=$(echo "stdin test" | "$CFBOX" cat -)
if [[ "$actual" == "stdin test" ]]; then ((++pass)); else echo "FAIL [cat stdin]"; ((++fail)); fi

echo "cat: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
