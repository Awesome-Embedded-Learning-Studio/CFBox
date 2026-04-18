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
    actual=$("$CFBOX" tail "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# default: last 10
expected="3
4
5
6
7
8
9
10
11
12"
run_test "default" "$expected" "$tmpdir/nums.txt"

# -n 3
expected="10
11
12"
run_test "-n3" "$expected" -n 3 "$tmpdir/nums.txt"

# +N from line N
expected="3
4
5
6
7
8
9
10
11
12"
run_test "+3" "$expected" -n +3 "$tmpdir/nums.txt"

# -c 3: last 3 bytes of file are "12\n", $() strips trailing newline → "12"
actual=$("$CFBOX" tail -c 3 "$tmpdir/nums.txt")
if [[ "$actual" == "12" ]]; then ((++pass)); else echo "FAIL [-c3] got $(printf '%q' "$actual")"; ((++fail)); fi

# stdin
expected="b
c"
actual=$(printf 'a\nb\nc\n' | "$CFBOX" tail -n 2)
if [[ "$actual" == "$expected" ]]; then ((++pass)); else echo "FAIL [tail stdin]"; ((++fail)); fi

echo "tail: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
