#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

run_test() {
    local name="$1"; shift
    local expected="$1"; shift
    local actual
    actual=$("$CFBOX" echo "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# basic output
run_test "basic" "hello world" hello world

# -n no trailing newline (capture shows no trailing \n)
actual=$("$CFBOX" echo -n hello; echo "X")
if [[ "$actual" == "helloX" ]]; then ((++pass)); else echo "FAIL [-n]: got $(printf '%q' "$actual")"; ((++fail)); fi

# -e escape sequences
run_test "escape_tab" $'hello\tworld' -e 'hello\tworld'
run_test "escape_newline" $'hello\nworld' -e 'hello\nworld'
run_test "escape_backslash" $'hello\world' -e 'hello\\world'

# empty args
run_test "empty" "" ""

# -- stops option parsing
run_test "doubledash" "-n hello" -- -n hello

echo "echo: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
