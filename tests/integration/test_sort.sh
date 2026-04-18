#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "banana\napple\ncherry\n" > "$tmpdir/sort1.txt"
printf "3\n1\n2\n10\n" > "$tmpdir/sort2.txt"
printf "b a\na c\nc b\n" > "$tmpdir/sort3.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" sort "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# basic lexicographic sort
run_test "basic" "apple
banana
cherry" "$tmpdir/sort1.txt"

# reverse
run_test "reverse" "cherry
banana
apple" -r "$tmpdir/sort1.txt"

# numeric sort
run_test "numeric" "1
2
3
10" -n "$tmpdir/sort2.txt"

# stdin
run_test "stdin" "a
b
c" <<< "$(printf "c\nb\na\n")"

# unique
run_test "unique" "apple
banana
cherry" -u <<< "$(printf "banana\napple\ncherry\napple\nbanana\n")"

# -k field sort
run_test "key_field" "b a
c b
a c" -k 2 "$tmpdir/sort3.txt"

# multiple files — all lines merged then sorted lexicographically
run_test "multi_file" "1
10
2
3
apple
banana
cherry" "$tmpdir/sort1.txt" "$tmpdir/sort2.txt"

echo "sort: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
