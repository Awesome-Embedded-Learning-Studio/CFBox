#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "hello world\nfoo bar\nbaz qux\nhello again\n" > "$tmpdir/grep1.txt"
printf "Hello World\nFOO BAR\n" > "$tmpdir/grep2.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" grep "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# basic match
run_test "basic" "hello world
hello again" "hello" "$tmpdir/grep1.txt"

# -i ignore case
run_test "ignore_case" "hello world
hello again" -i "hello" "$tmpdir/grep1.txt"

# -v invert match
run_test "invert" "foo bar
baz qux" -v "hello" "$tmpdir/grep1.txt"

# -n line numbers
run_test "line_numbers" "1:hello world
4:hello again" -n "hello" "$tmpdir/grep1.txt"

# -c count
run_test "count" "2" -c "hello" "$tmpdir/grep1.txt"

# -l files with matches
run_test "files_with" "$tmpdir/grep1.txt" -l "hello" "$tmpdir/grep1.txt" "$tmpdir/grep2.txt"

# multiple files — filename prefix
run_test "multi_file" "$tmpdir/grep1.txt:hello world
$tmpdir/grep1.txt:hello again" "hello" "$tmpdir/grep1.txt" "$tmpdir/grep2.txt"

# -E extended regex
run_test "extended" "hello world
baz qux
hello again" -E "hello|baz" "$tmpdir/grep1.txt"

# stdin
run_test "stdin" "hello world
hello again" "hello" <<< "$(cat "$tmpdir/grep1.txt")"

# -r recursive
mkdir -p "$tmpdir/subdir"
printf "found_it\nnope\n" > "$tmpdir/subdir/deep.txt"
run_test "recursive" "$tmpdir/subdir/deep.txt:found_it" -r "found_it" "$tmpdir"

# -q quiet — exit code 0
set +e
"$CFBOX" grep -q "hello" "$tmpdir/grep1.txt"
rc=$?
set -e
if [[ "$rc" -eq 0 ]]; then
    ((++pass))
else
    echo "FAIL [quiet_match]: expected exit 0, got $rc"
    ((++fail))
fi

# -q quiet — exit code 1 (no match)
set +e
"$CFBOX" grep -q "nonexistent" "$tmpdir/grep1.txt"
rc=$?
set -e
if [[ "$rc" -eq 1 ]]; then
    ((++pass))
else
    echo "FAIL [quiet_nomatch]: expected exit 1, got $rc"
    ((++fail))
fi

echo "grep: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
