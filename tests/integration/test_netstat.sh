#!/usr/bin/env bash
# test_netstat.sh — netstat prints headers and lists live sockets (read-only).
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

# default (tcp+udp) prints the inet header
out=$("$CFBOX" netstat 2>&1) || true
if echo "$out" | grep -q "Active Internet connections" && echo "$out" | grep -q "Local Address"; then
    ((++pass))
else
    echo "FAIL [netstat default]: no inet header"
    ((++fail))
fi

# -a includes servers (LISTEN). A Linux box almost always has something listening.
out=$("$CFBOX" netstat -a 2>&1) || true
if echo "$out" | grep -q "servers and established"; then
    ((++pass))
else
    echo "FAIL [netstat -a]: no 'servers and established' header"
    ((++fail))
fi

# -x emits the unix section
out=$("$CFBOX" netstat -x 2>&1) || true
if echo "$out" | grep -q "Active UNIX domain sockets"; then
    ((++pass))
else
    echo "FAIL [netstat -x]: no unix header"
    ((++fail))
fi

echo "netstat: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
