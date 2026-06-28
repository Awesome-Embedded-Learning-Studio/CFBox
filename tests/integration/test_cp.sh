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

# cp -a: preserve mode/time + copy symlinks as links (never follow)
mkdir -p "$tmpdir/archsrc/sub"
echo "data" > "$tmpdir/archsrc/file.txt"
echo "inner" > "$tmpdir/archsrc/sub/inner.txt"
ln -s file.txt "$tmpdir/archsrc/link.txt"
ln -s /nonexistent "$tmpdir/archsrc/broken"
chmod 0640 "$tmpdir/archsrc/file.txt"
run_test "archive" 0 -a "$tmpdir/archsrc" "$tmpdir/archdst"

[[ -f "$tmpdir/archdst/file.txt" && -f "$tmpdir/archdst/sub/inner.txt" ]] && ((++pass)) || { echo "FAIL [cp -a structure]"; ((++fail)); }

src_mode=$(stat -c '%a' "$tmpdir/archsrc/file.txt")
dst_mode=$(stat -c '%a' "$tmpdir/archdst/file.txt")
[[ "$src_mode" == "$dst_mode" ]] && ((++pass)) || { echo "FAIL [cp -a mode $src_mode!=$dst_mode]"; ((++fail)); }

[[ -L "$tmpdir/archdst/link.txt" ]] && ((++pass)) || { echo "FAIL [cp -a symlink not a link]"; ((++fail)); }
src_link=$(readlink "$tmpdir/archsrc/link.txt")
dst_link=$(readlink "$tmpdir/archdst/link.txt")
[[ "$src_link" == "$dst_link" ]] && ((++pass)) || { echo "FAIL [cp -a link target $src_link!=$dst_link]"; ((++fail)); }

[[ -L "$tmpdir/archdst/broken" ]] && ((++pass)) || { echo "FAIL [cp -a broken symlink]"; ((++fail)); }

# cp no operand
run_test "no_operand" 1

echo "cp: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
