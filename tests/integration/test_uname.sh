#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# uname -s should match system uname -s
out=$("$CFBOX" uname -s)
expected=$(uname -s)
assert_output "$expected" "$out"
((++pass))

# uname -m: just verify non-empty (differs under QEMU cross-arch)
out=$("$CFBOX" uname -m)
[[ -n "$out" ]]
((++pass))

# default = -s
out=$("$CFBOX" uname)
expected=$(uname -s)
assert_output "$expected" "$out"
((++pass))

assert_exit 0 uname --help
((++pass))

echo "uname: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
