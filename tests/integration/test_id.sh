#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# Default format contains uid= and gid=
out=$("$CFBOX" id)
[[ "$out" == uid=* ]]
((++pass))
[[ "$out" == *gid=* ]]
((++pass))

# -u prints numeric UID
out=$("$CFBOX" id -u)
expected=$(id -u)
assert_output "$expected" "$out"
((++pass))

# -g prints numeric GID
out=$("$CFBOX" id -g)
expected=$(id -g)
assert_output "$expected" "$out"
((++pass))

# -u -n prints username
out=$("$CFBOX" id -u -n)
expected=$(id -u -n)
assert_output "$expected" "$out"
((++pass))

# -G prints all groups
out=$("$CFBOX" id -G)
expected=$(id -G)
assert_output "$expected" "$out"
((++pass))

echo "id: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
