#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n" > "$tmpdir/nums.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" head "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# default: first 10 lines
expected="1
2
3
4
5
6
7
8
9
10"
run_test "default" "$expected" "$tmpdir/nums.txt"

# -n 3
expected="1
2
3"
run_test "-n3" "$expected" -n 3 "$tmpdir/nums.txt"

# -c 5
expected=$'1\n2\n3'
actual=$("$CFBOX" head -c 5 "$tmpdir/nums.txt")
if [[ "$actual" == $'1\n2\n3' ]]; then ((++pass)); else echo "FAIL [-c5] got $(printf '%q' "$actual")"; ((++fail)); fi

# stdin
expected="a
b"
actual=$(printf 'a\nb\nc\n' | "$CFBOX" head -n 2)
if [[ "$actual" == "$expected" ]]; then ((++pass)); else echo "FAIL [head stdin]"; ((++fail)); fi

echo "head: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
