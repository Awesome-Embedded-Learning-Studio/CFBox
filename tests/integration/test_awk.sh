#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# basic print
actual=$(echo -e "hello world\nfoo bar" | "$CFBOX" awk '{print $1}')
expected=$'hello\nfoo'
if [[ "$actual" == "$expected" ]]; then ((++pass)); else echo "FAIL [awk print]: got $(printf '%q' "$actual")"; ((++fail)); fi

# BEGIN block
actual=$("$CFBOX" awk 'BEGIN{print 42}')
if [[ "$actual" == "42" ]]; then ((++pass)); else echo "FAIL [awk BEGIN]: got $(printf '%q' "$actual")"; ((++fail)); fi

# sum
actual=$(echo -e "10\n20\n30" | "$CFBOX" awk '{s+=$1}END{print s}')
if [[ "$actual" == "60" ]]; then ((++pass)); else echo "FAIL [awk sum]: got $(printf '%q' "$actual")"; ((++fail)); fi

# -F separator
actual=$(echo "a:b:c" | "$CFBOX" awk -F: '{print $2}')
if [[ "$actual" == "b" ]]; then ((++pass)); else echo "FAIL [awk -F]: got $(printf '%q' "$actual")"; ((++fail)); fi

# printf
actual=$("$CFBOX" awk 'BEGIN{printf "%d %s\n", 1, "hi"}')
if [[ "$actual" == "1 hi" ]]; then ((++pass)); else echo "FAIL [awk printf]: got $(printf '%q' "$actual")"; ((++fail)); fi

echo "awk: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
