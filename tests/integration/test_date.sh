#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# date outputs something
out=$("$CFBOX" date)
[[ -n "$out" ]]
((++pass))

# date +%Y is 4 digits
out=$("$CFBOX" date +%Y)
if [[ "$out" =~ ^[0-9]{4}$ ]]; then ((++pass)); else echo "FAIL [date +%Y]: expected 4 digits, got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 date --help
((++pass))

echo "date: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
