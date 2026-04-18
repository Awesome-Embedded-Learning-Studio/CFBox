#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" printf "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

run_test "string" "hello world" '%s %s' hello world
run_test "int" "42" '%d' 42
run_test "float" "3.14" '%.2f' 3.14159
run_test "char" "A" '%c' ABC
run_test "percent" "%" '%%'
# escape_newline: $() strips trailing newline, so add X marker
actual=$("$CFBOX" printf 'hello\n'; echo X)
if [[ "$actual" == $'hello\nX' ]]; then ((++pass)); else echo "FAIL [escape_newline] got $(printf '%q' "$actual")"; ((++fail)); fi
run_test "reuse_format" "abc" '%s' a b c

echo "printf: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
