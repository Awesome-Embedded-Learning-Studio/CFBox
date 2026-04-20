#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

run_test() {
    local name="$1" expected="$2"; shift 2
    local actual
    actual=$("$CFBOX" basename "$@")
    if [[ "$expected" == "$actual" ]]; then ((++pass))
    else echo "FAIL [$name]: expected='$expected' actual='$actual'"; ((++fail))
    fi
}

run_test "basic"        "bar"       /foo/bar
run_test "trailing_slash" "bar"     /foo/bar/
run_test "no_dir"       "bar"       bar
run_test "root"         "/"         /
run_test "with_suffix"  "bar"       /foo/bar.c .c
run_test "no_suffix"    "bar"       /foo/bar .c
run_test "suffix_is_name" ".c"      /foo/.c .c

assert_exit 1 basename

echo "basename: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
