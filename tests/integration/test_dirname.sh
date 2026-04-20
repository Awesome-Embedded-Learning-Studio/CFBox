#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

run_test() {
    local name="$1" expected="$2"; shift 2
    local actual
    actual=$("$CFBOX" dirname "$@")
    if [[ "$expected" == "$actual" ]]; then ((++pass))
    else echo "FAIL [$name]: expected='$expected' actual='$actual'"; ((++fail))
    fi
}

run_test "basic"           "/foo"     /foo/bar
run_test "trailing_slash"  "/foo"     /foo/bar/
run_test "no_dir"          "."        bar
run_test "root_child"      "/"        /foo
run_test "root"            "/"        /
run_test "dot"             "."        .
run_test "dotdot"          "."        ..

assert_exit 1 dirname

echo "dirname: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
