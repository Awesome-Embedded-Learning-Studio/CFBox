#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

out=$("$CFBOX" nproc)
[[ "$out" -gt 0 ]]
((++pass))

out=$("$CFBOX" nproc --ignore=999)
assert_output "1" "$out"
((++pass))

assert_exit 0 nproc --help
((++pass))

echo "nproc: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
