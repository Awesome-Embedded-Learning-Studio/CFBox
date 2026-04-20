#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# pwd output matches shell pwd
out=$("$CFBOX" pwd)
expected=$(pwd)
assert_output "$expected" "$out"
((++pass))

# pwd -L
out=$("$CFBOX" pwd -L)
assert_output "$expected" "$out"
((++pass))

# pwd -P
out=$("$CFBOX" pwd -P)
# -P resolves symlinks so it may differ; just check non-empty
[[ -n "$out" ]]
((++pass))

# --help
assert_exit 0 pwd --help
((++pass))

echo "pwd: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
