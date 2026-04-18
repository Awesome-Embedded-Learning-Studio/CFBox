#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# Setup test directory structure
mkdir -p "$tmpdir/lsdir"
echo "file1" > "$tmpdir/lsdir/file1.txt"
echo "file2" > "$tmpdir/lsdir/file2.txt"
echo "hidden" > "$tmpdir/lsdir/.hidden"
mkdir "$tmpdir/lsdir/subdir"
chmod +x "$tmpdir/lsdir/subdir"

run_test() {
    local name="$1"; shift
    local actual
    actual=$("$CFBOX" ls "$@" 2>/dev/null)
    echo "$actual"
}

# basic ls (no hidden)
actual=$("$CFBOX" ls "$tmpdir/lsdir")
if [[ "$actual" == *"file1.txt"* ]] && [[ "$actual" == *"file2.txt"* ]] && [[ "$actual" == *"subdir"* ]] && [[ "$actual" != *".hidden"* ]]; then
    ((++pass))
else
    echo "FAIL [ls basic]: got $(printf '%q' "$actual")"
    ((++fail))
fi

# ls -a (with hidden)
actual=$("$CFBOX" ls -a "$tmpdir/lsdir")
if [[ "$actual" == *".hidden"* ]] && [[ "$actual" == *"file1.txt"* ]]; then
    ((++pass))
else
    echo "FAIL [ls -a]: got $(printf '%q' "$actual")"
    ((++fail))
fi

# ls -l (long format)
actual=$("$CFBOX" ls -l "$tmpdir/lsdir")
if [[ "$actual" == *"file1.txt"* ]] && [[ "$actual" == *"-"*"r"* ]]; then
    ((++pass))
else
    echo "FAIL [ls -l]: got $(printf '%q' "$actual")"
    ((++fail))
fi

# ls -lh (human readable)
actual=$("$CFBOX" ls -lh "$tmpdir/lsdir")
if [[ "$actual" == *"file1.txt"* ]]; then
    ((++pass))
else
    echo "FAIL [ls -lh]: got $(printf '%q' "$actual")"
    ((++fail))
fi

# ls single file
actual=$("$CFBOX" ls "$tmpdir/lsdir/file1.txt")
if [[ "$actual" == *"file1.txt"* ]]; then
    ((++pass))
else
    echo "FAIL [ls file]: got $(printf '%q' "$actual")"
    ((++fail))
fi

# ls nonexistent
set +e
"$CFBOX" ls "$tmpdir/no_such_dir" 2>/dev/null
exit_code=$?
set -e
if [[ "$exit_code" -ne 0 ]]; then
    ((++pass))
else
    echo "FAIL [ls nonexistent should fail]"
    ((++fail))
fi

# ls current dir
actual=$("$CFBOX" ls)
if [[ -n "$actual" ]]; then
    ((++pass))
else
    echo "FAIL [ls current dir empty]"
    ((++fail))
fi

echo "ls: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
