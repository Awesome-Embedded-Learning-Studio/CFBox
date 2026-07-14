#!/usr/bin/env bash
# Run all differential tests (cfbox vs busybox). NOT in CI by default — needs
# a built busybox oracle (competition/busybox/busybox).
set -euo pipefail
cd "$(dirname "$0")"

fail=0
total_pass=0
for t in test_diff_*.sh; do
    echo "=== $t ==="
    if bash "$t"; then
        :
    else
        fail=1
    fi
done

if [[ $fail -eq 0 ]]; then
    echo "All differential tests passed."
else
    echo "Some differential tests reported divergences (see FAIL lines above)."
fi
exit $fail
