#!/usr/bin/env bash
# test_ping.sh — ping needs CAP_NET_RAW. Without it → clear EPERM + exit 2.
# With it (qemu-system / root) → a 127.0.0.1 self-ping yields icmp_seq. Both pass.
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# Either we lack CAP_NET_RAW (EPERM, exit 2) or we have it (reply line). Both OK.
out=$("$CFBOX" ping -c 1 -W 1 127.0.0.1 2>&1) || true
if echo "$out" | grep -qi "Operation not permitted\|SOCK_RAW\|CAP_NET_RAW"; then
    ((++pass))
elif echo "$out" | grep -q "icmp_seq=1"; then
    ((++pass))
else
    echo "FAIL [ping 127.0.0.1]: neither EPERM nor reply — $out"
    ((++fail))
fi

# no host → usage error exit 2
if ! "$CFBOX" ping >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [ping no host]: should exit non-zero"
    ((++fail))
fi

# invalid -c value → exit 2
if ! "$CFBOX" ping -c abc 127.0.0.1 >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [ping bad -c]: should exit non-zero"
    ((++fail))
fi

echo "ping: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
