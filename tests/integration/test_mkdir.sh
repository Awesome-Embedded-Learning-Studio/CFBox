#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

run_test() {
    local name="$1"; shift
    local expected_exit="$1"; shift
    set +e
    "$CFBOX" mkdir "$@" 2>/dev/null
    local actual_exit=$?
    set -e
    if [[ "$expected_exit" == "$actual_exit" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected exit $expected_exit, got $actual_exit"
        ((++fail))
    fi
}

# basic mkdir
run_test "basic" 0 "$tmpdir/test_basic"
[[ -d "$tmpdir/test_basic" ]] && ((++pass)) || { echo "FAIL [mkdir basic dir missing]"; ((++fail)); }

# mkdir -p recursive
run_test "recursive" 0 -p "$tmpdir/test_recursive/a/b/c"
[[ -d "$tmpdir/test_recursive/a/b/c" ]] && ((++pass)) || { echo "FAIL [mkdir -p dirs missing]"; ((++fail)); }

# mkdir existing dir should fail
run_test "exists" 1 "$tmpdir/test_basic"

# mkdir -m mode
run_test "mode" 0 -m 700 "$tmpdir/test_mode"
[[ -d "$tmpdir/test_mode" ]] && ((++pass)) || { echo "FAIL [mkdir -m dir missing]"; ((++fail)); }

# mkdir no operand
run_test "no_operand" 1

echo "mkdir: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
