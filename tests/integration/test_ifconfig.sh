#!/usr/bin/env bash
# test_ifconfig.sh — ifconfig display: -a lists lo, named interface works,
# bogus interface exits non-zero.
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# ifconfig -a must list lo (every Linux has it) with the encap line
out=$("$CFBOX" ifconfig -a 2>&1) || true
if echo "$out" | grep -q "lo" && echo "$out" | grep -q "Link encap"; then
    ((++pass))
else
    echo "FAIL [ifconfig -a]: missing lo or Link encap"
    ((++fail))
fi

# no-arg run still shows at least one interface (loopback is up)
out=$("$CFBOX" ifconfig 2>&1) || true
if echo "$out" | grep -q "Link encap"; then
    ((++pass))
else
    echo "FAIL [ifconfig no-arg]"
    ((++fail))
fi

# named interface: lo shows loopback markers
out=$("$CFBOX" ifconfig lo 2>&1) || true
if echo "$out" | grep -q "Local Loopback"; then
    ((++pass))
else
    echo "FAIL [ifconfig lo]: $(echo "$out" | head -1)"
    ((++fail))
fi

# bogus interface → non-zero exit
if ! "$CFBOX" ifconfig no-such-iface-xyz >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [ifconfig bogus exit]"
    ((++fail))
fi

echo "ifconfig: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
