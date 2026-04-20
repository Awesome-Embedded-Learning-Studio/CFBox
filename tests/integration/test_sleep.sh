#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# sleep 0 returns immediately
assert_exit 0 sleep 0
((++pass))

# sleep with fractional value
start=$(date +%s%N)
"$CFBOX" sleep 0.1
end=$(date +%s%N)
elapsed=$(( (end - start) / 1000000 ))  # ms
[[ $elapsed -ge 50 ]]  # at least ~50ms
((++pass))

# invalid argument
assert_exit 1 sleep abc
((++pass))

# missing operand
assert_exit 1 sleep
((++pass))

echo "sleep: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
