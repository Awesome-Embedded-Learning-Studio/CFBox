#!/usr/bin/env bash
# test_traceroute.sh — traceroute needs CAP_NET_RAW (ICMP socket). Without it →
# clear EPERM + exit 2. With it → a 127.0.0.1 self-trace prints the banner.
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# Either EPERM or a real trace banner — both pass.
out=$("$CFBOX" traceroute -m 3 -q 1 -w 1 -n 127.0.0.1 2>&1) || true
if echo "$out" | grep -qi "Operation not permitted\|SOCK_RAW\|CAP_NET_RAW"; then
    ((++pass))
elif echo "$out" | grep -q "traceroute to"; then
    ((++pass))
else
    echo "FAIL [traceroute 127.0.0.1]: neither EPERM nor banner — $out"
    ((++fail))
fi

# no host → exit 2
if ! "$CFBOX" traceroute >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [traceroute no host]: should exit non-zero"
    ((++fail))
fi

# invalid -m → exit 2
if ! "$CFBOX" traceroute -m abc 127.0.0.1 >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [traceroute bad -m]: should exit non-zero"
    ((++fail))
fi

echo "traceroute: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
