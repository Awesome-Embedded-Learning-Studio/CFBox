#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

mkdir "$tmpdir/src"
echo "file1" > "$tmpdir/src/a.txt"
echo "file2" > "$tmpdir/src/b.txt"

# create and list
"$CFBOX" tar -cf "$tmpdir/test.tar" -C "$tmpdir" src
listing=$("$CFBOX" tar -tf "$tmpdir/test.tar")
if echo "$listing" | grep -q "a.txt" && echo "$listing" | grep -q "b.txt"; then
    ((++pass))
else
    echo "FAIL [tar list]: $listing"; ((++fail))
fi

# extract and verify
mkdir "$tmpdir/out"
(cd "$tmpdir/out" && "$CFBOX" tar -xf "$tmpdir/test.tar")
if [[ -f "$tmpdir/out/src/a.txt" ]] && [[ $(cat "$tmpdir/out/src/a.txt") == "file1" ]]; then
    ((++pass))
else
    echo "FAIL [tar extract]"; ((++fail))
fi

echo "tar: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
