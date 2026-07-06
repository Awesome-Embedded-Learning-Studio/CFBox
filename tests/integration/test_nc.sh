#!/usr/bin/env bash
# test_nc.sh — nc loopback echo: server listens, client sends one line,
# server writes what it received to a file we then check.
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

PORT=29501

# Server: listen, relay stdin (here /dev/null → immediate EOF → SHUT_WR) to the
# accepted socket, and write the socket's data to a file via stdout redirect.
"$CFBOX" nc -l -p "$PORT" </dev/null >"$tmpdir/out" 2>"$tmpdir/serr" &
server=$!
sleep 0.3

if ! kill -0 "$server" 2>/dev/null; then
    echo "nc: cannot listen on $PORT (busy?), SKIP ($(cat "$tmpdir/serr"))"
    exit 0
fi

# Client: send one line.
echo "hello-from-nc" | "$CFBOX" nc 127.0.0.1 "$PORT" 2>"$tmpdir/cerr" || true
sleep 0.3

if grep -q "hello-from-nc" "$tmpdir/out"; then
    ((++pass))
else
    echo "FAIL [nc echo]: out='$(cat "$tmpdir/out")' serr='$(cat "$tmpdir/serr")' cerr='$(cat "$tmpdir/cerr")'"
    ((++fail))
fi

kill "$server" 2>/dev/null || true
wait "$server" 2>/dev/null || true

echo "nc: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
