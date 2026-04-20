#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# yes with no args prints "y"
out=$(set +o pipefail; "$CFBOX" yes | head -n 3)
expected=$'y\ny\ny'
assert_output "$expected" "$out"
((++pass))

# yes with args prints them space-separated
out=$(set +o pipefail; "$CFBOX" yes hello world | head -n 2)
expected=$'hello world\nhello world'
assert_output "$expected" "$out"
((++pass))

assert_exit 0 yes --help
((++pass))

assert_exit 0 yes --version
((++pass))

echo "yes: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
