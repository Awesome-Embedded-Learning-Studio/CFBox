#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

run_test() {
    local name="$1" expected="$2"; shift 2
    local actual
    actual=$("$CFBOX" test "$@" && echo 0 || echo $?)
    # actual captures the exit code
    if [[ "$expected" == "0" ]]; then
        "$CFBOX" test "$@" ; ((++pass)) || { echo "FAIL [$name]: expected 0"; ((++fail)); }
    else
        ! "$CFBOX" test "$@" ; ((++pass)) || { echo "FAIL [$name]: expected nonzero"; ((++fail)); }
    fi
}

# Empty = false
"$CFBOX" test && { echo "FAIL: empty should be false"; ((++fail)); } || ((++pass))

# Non-empty = true
"$CFBOX" test "hello" ; ((++pass)) || { echo "FAIL: non-empty"; ((++fail)); }

# -z
"$CFBOX" test -z "" ; ((++pass)) || { echo "FAIL: -z empty"; ((++fail)); }
"$CFBOX" test -z "abc" && { echo "FAIL: -z non-empty"; ((++fail)); } || ((++pass))

# -n
"$CFBOX" test -n "abc" ; ((++pass)) || { echo "FAIL: -n non-empty"; ((++fail)); }
"$CFBOX" test -n "" && { echo "FAIL: -n empty"; ((++fail)); } || ((++pass))

# String comparison
"$CFBOX" test "abc" = "abc" ; ((++pass)) || { echo "FAIL: = equal"; ((++fail)); }
"$CFBOX" test "abc" != "def" ; ((++pass)) || { echo "FAIL: != different"; ((++fail)); }

# Integer comparison
"$CFBOX" test 1 -eq 1 ; ((++pass)) || { echo "FAIL: -eq"; ((++fail)); }
"$CFBOX" test 1 -ne 2 ; ((++pass)) || { echo "FAIL: -ne"; ((++fail)); }
"$CFBOX" test 1 -lt 2 ; ((++pass)) || { echo "FAIL: -lt"; ((++fail)); }
"$CFBOX" test 2 -gt 1 ; ((++pass)) || { echo "FAIL: -gt"; ((++fail)); }
"$CFBOX" test 1 -le 1 ; ((++pass)) || { echo "FAIL: -le"; ((++fail)); }
"$CFBOX" test 2 -ge 1 ; ((++pass)) || { echo "FAIL: -ge"; ((++fail)); }

# File tests
"$CFBOX" test -e /dev/null ; ((++pass)) || { echo "FAIL: -e /dev/null"; ((++fail)); }
"$CFBOX" test -d /tmp ; ((++pass)) || { echo "FAIL: -d /tmp"; ((++fail)); }
"$CFBOX" test -f /dev/null && { echo "FAIL: -f /dev/null should be false"; ((++fail)); } || ((++pass))

# Not
"$CFBOX" test ! "" ; ((++pass)) || { echo "FAIL: ! empty"; ((++fail)); }

# And
"$CFBOX" test "a" = "a" -a "1" -eq 1 ; ((++pass)) || { echo "FAIL: -a true"; ((++fail)); }

# Or
"$CFBOX" test "a" = "b" -o "1" -eq 1 ; ((++pass)) || { echo "FAIL: -o true"; ((++fail)); }

# Bracket form
"$CFBOX" [ "abc" = "abc" ] ; ((++pass)) || { echo "FAIL: [ ] form"; ((++fail)); }

echo "test: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
