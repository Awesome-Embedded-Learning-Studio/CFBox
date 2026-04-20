#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# expr 2 + 3 = "5"
out=$("$CFBOX" expr 2 + 3)
if [[ "$out" == "5" ]]; then ((++pass)); else echo "FAIL [expr 2+3]: expected '5', got $(printf '%q' "$out")"; ((++fail)); fi

# expr 10 - 3 = "7"
out=$("$CFBOX" expr 10 - 3)
if [[ "$out" == "7" ]]; then ((++pass)); else echo "FAIL [expr 10-3]: expected '7', got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 expr --help
((++pass))

echo "expr: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
