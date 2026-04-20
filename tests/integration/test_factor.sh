#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# factor 42 = "42: 2 3 7"
out=$("$CFBOX" factor 42)
if [[ "$out" == "42: 2 3 7" ]]; then ((++pass)); else echo "FAIL [factor 42]: expected '42: 2 3 7', got $(printf '%q' "$out")"; ((++fail)); fi

# factor 7 = "7: 7"
out=$("$CFBOX" factor 7)
if [[ "$out" == "7: 7" ]]; then ((++pass)); else echo "FAIL [factor 7]: expected '7: 7', got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 factor --help
((++pass))

echo "factor: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
