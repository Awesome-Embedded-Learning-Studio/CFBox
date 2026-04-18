#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

run_test() {
    local name="$1"; shift
    local expected_exit="$1"; shift
    set +e
    "$CFBOX" rm "$@" 2>/dev/null
    local actual_exit=$?
    set -e
    if [[ "$expected_exit" == "$actual_exit" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected exit $expected_exit, got $actual_exit"
        ((++fail))
    fi
}

# basic rm file
echo "hello" > "$tmpdir/file1.txt"
run_test "basic_file" 0 "$tmpdir/file1.txt"
[[ ! -f "$tmpdir/file1.txt" ]] && ((++pass)) || { echo "FAIL [rm file still exists]"; ((++fail)); }

# rm nonexistent (should fail without -f)
run_test "nonexistent" 1 "$tmpdir/no_such_file"

# rm -f nonexistent (should succeed)
run_test "force_nonexistent" 0 -f "$tmpdir/no_such_file"

# rm -r directory
mkdir -p "$tmpdir/dir1/sub"
echo "inner" > "$tmpdir/dir1/sub/file.txt"
run_test "recursive" 0 -r "$tmpdir/dir1"
[[ ! -d "$tmpdir/dir1" ]] && ((++pass)) || { echo "FAIL [rm -r dir still exists]"; ((++fail)); }

# rm directory without -r (should fail)
mkdir -p "$tmpdir/dir2"
run_test "dir_no_r" 1 "$tmpdir/dir2"

# rm / safety check
run_test "root_safety" 1 -r /

# rm no operand
run_test "no_operand" 1

echo "rm: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
