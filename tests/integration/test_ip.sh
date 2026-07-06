#!/usr/bin/env bash
# test_ip.sh — ip addr show lists lo with 127.0.0.1; no-subcommand exits 2.
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

out=$("$CFBOX" ip addr show 2>&1) || true
if echo "$out" | grep -q "lo" && echo "$out" | grep -q "127.0.0.1"; then
    ((++pass))
else
    echo "FAIL [ip addr show]: $(echo "$out" | head -3)"
    ((++fail))
fi

out=$("$CFBOX" ip addr 2>&1) || true
if echo "$out" | grep -q "LOOPBACK"; then
    ((++pass))
else
    echo "FAIL [ip addr]"
    ((++fail))
fi

if ! "$CFBOX" ip >/dev/null 2>&1; then
    ((++pass))
else
    echo "FAIL [ip no-subcommand should exit non-zero]"
    ((++fail))
fi

echo "ip: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
