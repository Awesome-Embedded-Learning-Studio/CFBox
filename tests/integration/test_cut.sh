#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# cut -d: -f2 produces "b"
out=$(echo "a:b:c" | "$CFBOX" cut -d: -f2)
if [[ "$out" == "b" ]]; then ((++pass)); else echo "FAIL [cut -d: -f2]: expected 'b', got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 cut --help
((++pass))

echo "cut: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
