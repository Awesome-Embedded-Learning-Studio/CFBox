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

# write path: needs CAP_NET_ADMIN. Two branches — non-root must report EPERM
# (never silently succeed), root round-trips MTU on loopback (safe + reversible).
if [ "$(id -u)" -ne 0 ]; then
    out=$("$CFBOX" ifconfig lo 127.0.0.1 2>&1) || true
    if echo "$out" | grep -qi "Operation not permitted\|Permission denied"; then
        ((++pass))
    else
        echo "FAIL [ifconfig write non-root]: expected EPERM, got: $out"
        ((++fail))
    fi
else
    orig=$("$CFBOX" ifconfig lo 2>&1 | grep -o 'MTU:[0-9]*' | head -1)
    "$CFBOX" ifconfig lo mtu 1280 2>&1 || true
    out=$("$CFBOX" ifconfig lo 2>&1) || true
    if echo "$out" | grep -q "MTU:1280"; then
        ((++pass))
    else
        echo "FAIL [ifconfig write root]: MTU not applied — $(echo "$out" | head -1)"
        ((++fail))
    fi
    # restore original MTU
    [ -n "$orig" ] && "$CFBOX" ifconfig lo mtu "${orig#MTU:}" 2>/dev/null || true
fi

echo "ifconfig: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
