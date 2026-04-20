#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# printenv outputs lines
out=$("$CFBOX" printenv)
[[ -n "$out" ]]
((++pass))

# printenv HOME is not empty
out=$("$CFBOX" printenv HOME)
[[ -n "$out" ]]
((++pass))

# printenv NONEXISTENT_VAR exits 1
set +e
"$CFBOX" printenv NONEXISTENT_VAR >/dev/null 2>&1
rc=$?
set -e
if [[ $rc -ne 0 ]]; then ((++pass)); else echo "FAIL: printenv NONEXISTENT_VAR should fail"; ((++fail)); fi

# --help
assert_exit 0 printenv --help
((++pass))

# --version
assert_exit 0 printenv --version
((++pass))

echo "printenv: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
