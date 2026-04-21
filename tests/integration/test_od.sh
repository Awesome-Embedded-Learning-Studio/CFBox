#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# od produces output with addresses (hex offsets)
out=$(echo hello | "$CFBOX" od)
if [[ "$out" =~ [0-9a-f]+ ]]; then ((++pass)); else echo "FAIL [od output]: expected addresses, got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 od --help
((++pass))

echo "od: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
