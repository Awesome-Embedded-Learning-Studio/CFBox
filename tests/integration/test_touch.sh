#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# touch creates file
"$CFBOX" touch "$tmpdir/created.txt" >/dev/null 2>&1
if [[ -f "$tmpdir/created.txt" ]]; then ((++pass)); else echo "FAIL [touch create]: file not created"; ((++fail)); fi

# touch -c doesn't create file
"$CFBOX" touch -c "$tmpdir/noexist.txt" >/dev/null 2>&1
if [[ ! -f "$tmpdir/noexist.txt" ]]; then ((++pass)); else echo "FAIL [touch -c]: file should not exist"; ((++fail)); fi

# --help
assert_exit 0 touch --help
((++pass))

echo "touch: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
