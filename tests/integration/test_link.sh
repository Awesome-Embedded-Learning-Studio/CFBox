#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

echo "hello" > "$tmpdir/file1"

# Create hard link and check exit code
set +e
"$CFBOX" link "$tmpdir/file1" "$tmpdir/file2" >/dev/null 2>&1
rc=$?
set -e
if [[ $rc -eq 0 ]]; then ((++pass)); else echo "FAIL: link creation returned $rc"; ((++fail)); fi

# Both files should exist with same content
diff "$tmpdir/file1" "$tmpdir/file2"
((++pass))

# Same inode
inode1=$(stat -c '%i' "$tmpdir/file1")
inode2=$(stat -c '%i' "$tmpdir/file2")
assert_output "$inode1" "$inode2"
((++pass))

# Missing operand
set +e
"$CFBOX" link "$tmpdir/file1" >/dev/null 2>&1
rc=$?
set -e
if [[ $rc -ne 0 ]]; then ((++pass)); else echo "FAIL: link with missing operand should fail"; ((++fail)); fi

echo "link: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
