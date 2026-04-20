#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

CFBOX="${CFBOX:-./build/cfbox}"

pass=0
fail=0

# Test --help for each applet
for applet in echo printf cat head tail wc sort uniq mkdir rm cp mv ls grep find sed init; do
    if "$CFBOX" "$applet" --help >/dev/null 2>&1; then
        ((++pass))
    else
        echo "FAIL: $applet --help returned non-zero"
        ((++fail))
    fi

    # Verify help output contains the applet name
    out=$("$CFBOX" "$applet" --help 2>&1)
    if [[ "$out" == *"$applet"* ]]; then
        ((++pass))
    else
        echo "FAIL: $applet --help output does not contain applet name"
        ((++fail))
    fi
done

# Test --version for each applet
for applet in echo printf cat head tail wc sort uniq mkdir rm cp mv ls grep find sed init; do
    if "$CFBOX" "$applet" --version >/dev/null 2>&1; then
        ((++pass))
    else
        echo "FAIL: $applet --version returned non-zero"
        ((++fail))
    fi

    out=$("$CFBOX" "$applet" --version 2>&1)
    if [[ "$out" == "cfbox $applet"* ]]; then
        ((++pass))
    else
        echo "FAIL: $applet --version output unexpected: $out"
        ((++fail))
    fi
done

echo "--- help tests: $pass passed, $fail failed ---"
if [[ $fail -gt 0 ]]; then exit 1; fi
