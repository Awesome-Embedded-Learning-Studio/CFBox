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
    "$CFBOX" mv "$@" 2>/dev/null
    local actual_exit=$?
    set -e
    if [[ "$expected_exit" == "$actual_exit" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected exit $expected_exit, got $actual_exit"
        ((++fail))
    fi
}

# basic mv rename
echo "hello" > "$tmpdir/mv1.txt"
run_test "basic_rename" 0 "$tmpdir/mv1.txt" "$tmpdir/mv1_renamed.txt"
[[ ! -f "$tmpdir/mv1.txt" && -f "$tmpdir/mv1_renamed.txt" ]] && ((++pass)) || { echo "FAIL [mv rename]"; ((++fail)); }

# mv into directory
echo "world" > "$tmpdir/mv2.txt"
mkdir -p "$tmpdir/mvdir"
run_test "into_dir" 0 "$tmpdir/mv2.txt" "$tmpdir/mvdir/"
[[ ! -f "$tmpdir/mv2.txt" && -f "$tmpdir/mvdir/mv2.txt" ]] && ((++pass)) || { echo "FAIL [mv into dir]"; ((++fail)); }

# mv directory
mkdir -p "$tmpdir/src_mvdir"
echo "content" > "$tmpdir/src_mvdir/file.txt"
run_test "dir_move" 0 "$tmpdir/src_mvdir" "$tmpdir/dst_mvdir"
[[ ! -d "$tmpdir/src_mvdir" && -d "$tmpdir/dst_mvdir" && -f "$tmpdir/dst_mvdir/file.txt" ]] && ((++pass)) || { echo "FAIL [mv dir]"; ((++fail)); }

# mv -f force overwrite
echo "src" > "$tmpdir/force_src.txt"
echo "dst" > "$tmpdir/force_dst.txt"
run_test "force" 0 -f "$tmpdir/force_src.txt" "$tmpdir/force_dst.txt"
[[ "$(cat "$tmpdir/force_dst.txt")" == "src" ]] && ((++pass)) || { echo "FAIL [mv -f content]"; ((++fail)); }

# mv nonexistent (should fail)
run_test "nonexistent" 1 "$tmpdir/no_such_file" "$tmpdir/dest"

# mv no operand
run_test "no_operand" 1

echo "mv: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
