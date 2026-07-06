#!/usr/bin/env bash
# test_route.sh — route display prints the table header; add/del rejected.
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

out=$("$CFBOX" route -n 2>&1) || true
if echo "$out" | grep -q "Kernel IP routing table" && echo "$out" | grep -q "Destination"; then
    ((++pass))
else
    echo "FAIL [route -n]: no header"
    ((++fail))
fi

out=$("$CFBOX" route 2>&1) || true
if echo "$out" | grep -q "Iface"; then
    ((++pass))
else
    echo "FAIL [route bare]"
    ((++fail))
fi

# add/del are out of scope for this build → non-zero exit
if ! "$CFBOX" route add default gw 1.2.3.4 >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [route add should exit non-zero]"
    ((++fail))
fi

echo "route: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
