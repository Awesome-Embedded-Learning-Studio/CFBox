#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

assert_exit 1 false
((++pass))

assert_exit 0 false --help
((++pass))

assert_exit 0 false --version
((++pass))

assert_exit 1 false ignored
((++pass))

echo "false: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
