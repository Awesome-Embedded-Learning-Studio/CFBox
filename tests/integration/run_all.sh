#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
export CFBOX="${CFBOX:-$(cd "$script_dir/../.." && pwd)/build/cfbox}"

if [[ ! -x "$CFBOX" ]]; then
    echo "ERROR: cfbox not found at $CFBOX"
    echo "Run: cmake -B build && cmake --build build"
    exit 1
fi

total_pass=0 total_fail=0

for test_script in "$script_dir"/test_*.sh; do
    [[ -x "$test_script" ]] || chmod +x "$test_script"
    echo "=== Running $(basename "$test_script") ==="
    if "$test_script"; then
        echo "  PASSED"
    else
        echo "  FAILED"
        ((++total_fail))
    fi
    echo
done

if [[ $total_fail -eq 0 ]]; then
    echo "All integration tests passed!"
    exit 0
else
    echo "$total_fail test(s) failed."
    exit 1
fi
