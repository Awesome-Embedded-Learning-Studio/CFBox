#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "a\na\nb\nb\nb\nc\n" > "$tmpdir/uniq1.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" uniq "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# basic uniq
run_test "basic" "a
b
c" "$tmpdir/uniq1.txt"

# -c count
actual=$("$CFBOX" uniq -c "$tmpdir/uniq1.txt")
if [[ "$actual" == *"2 a"* ]] && [[ "$actual" == *"3 b"* ]] && [[ "$actual" == *"1 c"* ]]; then
    ((++pass))
else
    echo "FAIL [uniq -c]: got $(printf '%q' "$actual")"
    ((++fail))
fi

# -d only duplicates
run_test "duplicates" "a
b" -d "$tmpdir/uniq1.txt"

# -u only unique
run_test "unique_only" "c" -u "$tmpdir/uniq1.txt"

# stdin
run_test "stdin" "a
b" <<< "$(printf "a\na\nb\nb\n")"

# sort | uniq pipeline
actual=$(printf "b\na\nc\na\n" | "$CFBOX" sort | "$CFBOX" uniq -c)
if [[ "$actual" == *"2 a"* ]] && [[ "$actual" == *"1 b"* ]] && [[ "$actual" == *"1 c"* ]]; then
    ((++pass))
else
    echo "FAIL [sort|uniq -c]: got $(printf '%q' "$actual")"
    ((++fail))
fi

echo "uniq: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
