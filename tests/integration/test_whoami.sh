#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

out=$("$CFBOX" whoami)
expected=$(whoami)
assert_output "$expected" "$out"
((++pass))

assert_exit 0 whoami --help
((++pass))

echo "whoami: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
