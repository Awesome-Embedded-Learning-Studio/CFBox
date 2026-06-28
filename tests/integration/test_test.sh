#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

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

# --- POSIX three-state exit codes: 0 true / 1 false / 2 error ---
# invalid integer operand -> 2
assert_exit 2 test abc -eq abc && ((++pass)) || { echo "FAIL: abc -eq abc should be 2"; ((++fail)); }
assert_exit 2 test 5 -eq 5x && ((++pass)) || { echo "FAIL: 5 -eq 5x should be 2"; ((++fail)); }

# bare unary op without operand -> 2
assert_exit 2 test -z && ((++pass)) || { echo "FAIL: bare -z should be 2"; ((++fail)); }

# unknown operator -> 2
assert_exit 2 test -q foo && ((++pass)) || { echo "FAIL: -q should be 2"; ((++fail)); }

# stray close paren -> 2
assert_exit 2 test a ')' && ((++pass)) || { echo "FAIL: stray ) should be 2"; ((++fail)); }

# -h alias for -L (symlink)
ln -s /dev/null "$tmpdir/link"
assert_exit 0 test -h "$tmpdir/link" && ((++pass)) || { echo "FAIL: -h symlink"; ((++fail)); }

# string < > (byte order)
assert_exit 0 test "a" "<" "b" && ((++pass)) || { echo "FAIL: str <"; ((++fail)); }
assert_exit 1 test "b" "<" "a" && ((++pass)) || { echo "FAIL: str < false"; ((++fail)); }

# -ef same file
echo x > "$tmpdir/a.txt"
assert_exit 0 test "$tmpdir/a.txt" -ef "$tmpdir/a.txt" && ((++pass)) || { echo "FAIL: -ef same file"; ((++fail)); }

# nested parens with OR
assert_exit 0 test '(' a = a -o b = c ')' && ((++pass)) || { echo "FAIL: nested parens OR"; ((++fail)); }

echo "test: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
