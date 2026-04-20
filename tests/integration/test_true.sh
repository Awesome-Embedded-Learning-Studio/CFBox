#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

"$CFBOX" true
assert_exit 0 true
((++pass))

assert_exit 0 true --help
((++pass))

assert_exit 0 true random args ignored
((++pass))

echo "true: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
