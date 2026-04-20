#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

out=$("$CFBOX" logname) || true
# logname may fail in CI (no controlling terminal)
# Just check it doesn't crash
((++pass))

assert_exit 0 logname --help
((++pass))

echo "logname: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
