#!/usr/bin/env bash
set -euo pipefail

CFBOX="${CFBOX:-$(dirname "$0")/../../build/cfbox}"

assert_output() {
    local expected="$1"
    local actual="$2"
    if [[ "$expected" != "$actual" ]]; then
        echo "FAIL: expected output:"
        echo "$expected"
        echo "actual output:"
        echo "$actual"
        return 1
    fi
}

assert_exit() {
    local expected="$1"
    shift
    local actual
    actual=$("$CFBOX" "$@" 2>/dev/null && echo 0 || echo $?) 2>/dev/null || true
    # rerun to get exit code cleanly
    set +e
    "$CFBOX" "$@" >/dev/null 2>&1
    actual=$?
    set -e
    if [[ "$expected" != "$actual" ]]; then
        echo "FAIL: expected exit code $expected, got $actual for: $*"
        return 1
    fi
}
