#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# echo "hello" | xargs produces "hello"
out=$(echo "hello" | "$CFBOX" xargs)
if [[ "$out" == "hello" ]]; then ((++pass)); else echo "FAIL [xargs basic]: expected 'hello', got $(printf '%q' "$out")"; ((++fail)); fi

# printf "a\nb\n" | xargs -n1 produces "a\nb"
out=$(printf "a\nb\n" | "$CFBOX" xargs -n1)
expected="a
b"
if [[ "$out" == "$expected" ]]; then ((++pass)); else echo "FAIL [xargs -n1]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 xargs --help
((++pass))

echo "xargs: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
