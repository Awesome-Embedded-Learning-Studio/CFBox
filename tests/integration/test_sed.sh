#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "hello world\nfoo bar\nbaz qux\n" > "$tmpdir/sed1.txt"
printf "line1\nline2\nline3\nline4\nline5\n" > "$tmpdir/sed2.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" sed "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# basic substitution
run_test "basic_sub" "HELLO world
foo bar
baz qux" "s/hello/HELLO/" "$tmpdir/sed1.txt"

# global flag
run_test "global" "HELLO HELLO
foo bar
baz qux" "s/hello/HELLO/g" <<< "$(printf "hello hello\nfoo bar\nbaz qux\n")"

# -n suppress + p flag
run_test "suppress_p" "hello world" -n "s/hello/hello/p" "$tmpdir/sed1.txt"

# d delete line by address
run_test "delete_line" "foo bar
baz qux" "1d" "$tmpdir/sed1.txt"

# address range
run_test "range_sub" "line1
CHANGED2
CHANGED3
CHANGED4
line5" "2,4s/line/CHANGED/" "$tmpdir/sed2.txt"

# $ address (last line)
run_test "last_line" "hello world
foo bar
LAST" '$s/baz qux/LAST/' "$tmpdir/sed1.txt"

# p command (print)
run_test "print_cmd" "hello world
hello world
foo bar
foo bar
baz qux
baz qux" "1p;2p;3p" "$tmpdir/sed1.txt"

# substitution on specific line
run_test "line_sub" "hello world
foo REPLACED
baz qux" "2s/bar/REPLACED/" "$tmpdir/sed1.txt"

# multiple commands via -e
run_test "multi_e" "HELLO world
foo bar
baz qux" -e "s/hello/HELLO/" "$tmpdir/sed1.txt"

# stdin
run_test "stdin" "HELLO world
foo bar
baz qux" "s/hello/HELLO/" <<< "$(cat "$tmpdir/sed1.txt")"

# no match — line passes through unchanged
run_test "no_match" "hello world
foo bar
baz qux" "s/xyz/ABC/" "$tmpdir/sed1.txt"

# multiple substitutions
run_test "multi_sub" "HELLO world
FOO bar
BAZ qux" "s/hello/HELLO/;s/foo/FOO/;s/baz/BAZ/" "$tmpdir/sed1.txt"

# regex pattern
run_test "regex" "HELLO world
foo bar
baz qux" "s/h[a-z]*/HELLO/" "$tmpdir/sed1.txt"

# delete address range
run_test "delete_range" "line1
line5" "2,4d" "$tmpdir/sed2.txt"

echo "sed: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
