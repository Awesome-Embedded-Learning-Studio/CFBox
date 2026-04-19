#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

# Create test directory structure
mkdir -p "$tmpdir/subdir/deep"
touch "$tmpdir/file1.txt"
touch "$tmpdir/file2.cpp"
touch "$tmpdir/subdir/nested.txt"
touch "$tmpdir/subdir/deep/deep.txt"
ln -sf "$tmpdir/file1.txt" "$tmpdir/link1" 2>/dev/null || true

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" find "$@" 2>/dev/null | sort)
    local sorted_expected
    sorted_expected=$(echo "$expected" | sort)
    if [[ "$sorted_expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]:"
        echo "  expected:"
        echo "$sorted_expected" | sed 's/^/    /'
        echo "  actual:"
        echo "$actual" | sed 's/^/    /'
        ((++fail))
    fi
}

# basic find — all files in directory
run_test "basic_all" "$tmpdir
$tmpdir/file1.txt
$tmpdir/file2.cpp
$tmpdir/link1
$tmpdir/subdir
$tmpdir/subdir/nested.txt
$tmpdir/subdir/deep
$tmpdir/subdir/deep/deep.txt" "$tmpdir"

# -name glob
run_test "name_txt" "$tmpdir/file1.txt
$tmpdir/subdir/nested.txt
$tmpdir/subdir/deep/deep.txt" "$tmpdir" -name "*.txt"

# -name with ? wildcard
run_test "name_wildcard" "$tmpdir/file1.txt
$tmpdir/file2.cpp" "$tmpdir" -name "file?.*"

# -type f (regular files only)
run_test "type_f" "$tmpdir/file1.txt
$tmpdir/file2.cpp
$tmpdir/subdir/nested.txt
$tmpdir/subdir/deep/deep.txt" "$tmpdir" -type f

# -type d (directories only)
run_test "type_d" "$tmpdir
$tmpdir/subdir
$tmpdir/subdir/deep" "$tmpdir" -type d

# -type l (symlinks only)
run_test "type_l" "$tmpdir/link1" "$tmpdir" -type l

# -maxdepth 1
run_test "maxdepth1" "$tmpdir
$tmpdir/file1.txt
$tmpdir/file2.cpp
$tmpdir/link1
$tmpdir/subdir" "$tmpdir" -maxdepth 1

# -maxdepth 2
run_test "maxdepth2" "$tmpdir
$tmpdir/file1.txt
$tmpdir/file2.cpp
$tmpdir/link1
$tmpdir/subdir
$tmpdir/subdir/nested.txt
$tmpdir/subdir/deep" "$tmpdir" -maxdepth 2

# -name + -type f combined
run_test "name_and_type" "$tmpdir/file1.txt
$tmpdir/subdir/nested.txt
$tmpdir/subdir/deep/deep.txt" "$tmpdir" -name "*.txt" -type f

# -exec echo
result=$("$CFBOX" find "$tmpdir" -name "file1.txt" -exec echo "found" ";" 2>/dev/null)
count=$(echo "$result" | grep -c "found" || true)
if [[ "$count" -ge 1 ]]; then
    ((++pass))
else
    echo "FAIL [exec]: expected at least 1 'found', got: $result"
    ((++fail))
fi

# default path (current dir) — just test it doesn't crash
# Use CFBOX env var (respects QEMU wrapper) with absolute path
ABS_CFBOX="$(cd "$(dirname "$CFBOX")" && pwd)/$(basename "$CFBOX")"
cd "$tmpdir"
set +e
"$ABS_CFBOX" find -name "file1.txt" >/dev/null 2>&1
rc=$?
set -e
cd - >/dev/null
if [[ "$rc" -eq 0 ]]; then
    ((++pass))
else
    echo "FAIL [default_path]: expected exit 0, got $rc"
    ((++fail))
fi

echo "find: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
