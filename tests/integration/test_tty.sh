#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# tty when stdin is a pipe should say "not a tty"
out=$(set +o pipefail; echo hi | "$CFBOX" tty || true)
assert_output "not a tty" "$out"
((++pass))

# -s silent mode with pipe: no output
out=$(set +o pipefail; echo hi | "$CFBOX" tty -s || true)
assert_output "" "$out"
((++pass))

assert_exit 0 tty --help
((++pass))

echo "tty: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
