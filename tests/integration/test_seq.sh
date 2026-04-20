#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# seq 5 produces 5 lines
out=$("$CFBOX" seq 5)
lines=$(echo "$out" | wc -l)
if [[ "$lines" -eq 5 ]]; then ((++pass)); else echo "FAIL [seq 5 lines]: expected 5, got $lines"; ((++fail)); fi

# seq 2 4 produces 2 3 4
out=$("$CFBOX" seq 2 4)
expected="2
3
4"
if [[ "$out" == "$expected" ]]; then ((++pass)); else echo "FAIL [seq 2 4]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$out")"; ((++fail)); fi

# seq -s, 3 produces "1,2,3"
out=$("$CFBOX" seq -s, 3)
if [[ "$out" == "1,2,3" ]]; then ((++pass)); else echo "FAIL [seq -s, 3]: expected='1,2,3' actual=$(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 seq --help
((++pass))

echo "seq: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
