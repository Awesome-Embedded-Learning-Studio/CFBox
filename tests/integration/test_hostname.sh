#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

out=$("$CFBOX" hostname)
[[ -n "$out" ]]
((++pass))

out=$("$CFBOX" hostname -s)
[[ "$out" != *"."* ]]
((++pass))

assert_exit 0 hostname --help
((++pass))

echo "hostname: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
