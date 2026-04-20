#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# tr a-z A-Z produces HELLO
out=$(echo hello | "$CFBOX" tr a-z A-Z)
if [[ "$out" == "HELLO" ]]; then ((++pass)); else echo "FAIL [tr upper]: expected 'HELLO', got $(printf '%q' "$out")"; ((++fail)); fi

# tr -d l produces heo
out=$(echo hello | "$CFBOX" tr -d l)
if [[ "$out" == "heo" ]]; then ((++pass)); else echo "FAIL [tr -d]: expected 'heo', got $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 tr --help
((++pass))

echo "tr: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
