#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

echo "original" > "$tmpdir/file1"

# ln -s creates symlink
"$CFBOX" ln -s "$tmpdir/file1" "$tmpdir/link1" >/dev/null 2>&1
if [[ -L "$tmpdir/link1" ]]; then ((++pass)); else echo "FAIL [ln -s]: symlink not created"; ((++fail)); fi

# symlink points to correct content
out=$(cat "$tmpdir/link1")
if [[ "$out" == "original" ]]; then ((++pass)); else echo "FAIL [ln -s content]: expected 'original', got $(printf '%q' "$out")"; ((++fail)); fi

# ln creates hard link
"$CFBOX" ln "$tmpdir/file1" "$tmpdir/file2" >/dev/null 2>&1
if [[ -f "$tmpdir/file2" ]]; then ((++pass)); else echo "FAIL [ln hard]: hard link not created"; ((++fail)); fi

# same inode
inode1=$(stat -c '%i' "$tmpdir/file1")
inode2=$(stat -c '%i' "$tmpdir/file2")
if [[ "$inode1" == "$inode2" ]]; then ((++pass)); else echo "FAIL [ln inode]: inodes differ ($inode1 vs $inode2)"; ((++fail)); fi

# --help
assert_exit 0 ln --help
((++pass))

echo "ln: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
