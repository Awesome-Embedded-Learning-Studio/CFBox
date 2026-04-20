#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# mktemp creates temp file
out=$("$CFBOX" mktemp -p "$tmpdir")
if [[ -f "$out" ]]; then ((++pass)); else echo "FAIL [mktemp file]: not created at $(printf '%q' "$out")"; ((++fail)); fi

# mktemp -d creates temp dir
out=$("$CFBOX" mktemp -d -p "$tmpdir")
if [[ -d "$out" ]]; then ((++pass)); else echo "FAIL [mktemp -d]: dir not created at $(printf '%q' "$out")"; ((++fail)); fi

# --help
assert_exit 0 mktemp --help
((++pass))

echo "mktemp: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
