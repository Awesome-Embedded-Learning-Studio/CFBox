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
    "$CFBOX" cp "$@" 2>/dev/null
    local actual_exit=$?
    set -e
    if [[ "$expected_exit" == "$actual_exit" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected exit $expected_exit, got $actual_exit"
        ((++fail))
    fi
}

# basic cp file
echo "hello world" > "$tmpdir/src1.txt"
run_test "basic_file" 0 "$tmpdir/src1.txt" "$tmpdir/dst1.txt"
diff "$tmpdir/src1.txt" "$tmpdir/dst1.txt" && ((++pass)) || { echo "FAIL [cp content mismatch]"; ((++fail)); }

# cp into directory
mkdir -p "$tmpdir/destdir"
run_test "into_dir" 0 "$tmpdir/src1.txt" "$tmpdir/destdir/"
[[ -f "$tmpdir/destdir/src1.txt" ]] && ((++pass)) || { echo "FAIL [cp into dir missing]"; ((++fail)); }

# cp -r directory
mkdir -p "$tmpdir/srcdir/sub"
echo "inner" > "$tmpdir/srcdir/sub/file.txt"
run_test "recursive" 0 -r "$tmpdir/srcdir" "$tmpdir/dstdir"
diff <(cd "$tmpdir/srcdir" && find . -type f | sort) <(cd "$tmpdir/dstdir" && find . -type f | sort) && ((++pass)) || { echo "FAIL [cp -r structure mismatch]"; ((++fail)); }
diff "$tmpdir/srcdir/sub/file.txt" "$tmpdir/dstdir/sub/file.txt" && ((++pass)) || { echo "FAIL [cp -r content mismatch]"; ((++fail)); }

# cp -r into existing directory
mkdir -p "$tmpdir/existing_dir"
run_test "into_existing_dir" 0 -r "$tmpdir/srcdir" "$tmpdir/existing_dir"
[[ -d "$tmpdir/existing_dir/srcdir" ]] && ((++pass)) || { echo "FAIL [cp -r into existing dir]"; ((++fail)); }

# cp directory without -r (should fail)
mkdir -p "$tmpdir/onlydir"
run_test "no_r" 1 "$tmpdir/onlydir" "$tmpdir/should_not_exist"

# cp multiple files into directory
echo "a" > "$tmpdir/m1.txt"
echo "b" > "$tmpdir/m2.txt"
mkdir -p "$tmpdir/multi_dst"
run_test "multi_into_dir" 0 "$tmpdir/m1.txt" "$tmpdir/m2.txt" "$tmpdir/multi_dst"
[[ -f "$tmpdir/multi_dst/m1.txt" && -f "$tmpdir/multi_dst/m2.txt" ]] && ((++pass)) || { echo "FAIL [cp multi]"; ((++fail)); }

# cp no operand
run_test "no_operand" 1

echo "cp: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
