#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

printf "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n" > "$tmpdir/nums.txt"

run_test() {
    local name="$1"
    local expected="$2"
    shift 2
    local actual
    actual=$("$CFBOX" tail "$@")
    if [[ "$expected" == "$actual" ]]; then
        ((++pass))
    else
        echo "FAIL [$name]: expected=$(printf '%q' "$expected") actual=$(printf '%q' "$actual")"
        ((++fail))
    fi
}

# default: last 10
expected="3
4
5
6
7
8
9
10
11
12"
run_test "default" "$expected" "$tmpdir/nums.txt"

# -n 3
expected="10
11
12"
run_test "-n3" "$expected" -n 3 "$tmpdir/nums.txt"

# +N from line N
expected="3
4
5
6
7
8
9
10
11
12"
run_test "+3" "$expected" -n +3 "$tmpdir/nums.txt"

# -c 3: last 3 bytes of file are "12\n", $() strips trailing newline → "12"
actual=$("$CFBOX" tail -c 3 "$tmpdir/nums.txt")
if [[ "$actual" == "12" ]]; then ((++pass)); else echo "FAIL [-c3] got $(printf '%q' "$actual")"; ((++fail)); fi

# stdin
expected="b
c"
actual=$(printf 'a\nb\nc\n' | "$CFBOX" tail -n 2)
if [[ "$actual" == "$expected" ]]; then ((++pass)); else echo "FAIL [tail stdin]"; ((++fail)); fi

# --- follow (-f/-F): background process + appended data + SIGINT -----------
# Follow blocks, so each case spawns tail, appends, waits one poll interval
# (default -s 1), then signals SIGINT and checks output + exit code.
fpass=0 ffail=0

# -f: appended data is emitted and SIGINT exits 0
"$CFBOX" tail -f "$tmpdir/nums.txt" > "$tmpdir/fout.txt" 2>/dev/null &
fpid=$!
sleep 2
printf "13\n14\n" >> "$tmpdir/nums.txt"
sleep 2
kill -INT "$fpid" 2>/dev/null
wait "$fpid" 2>/dev/null; fcode=$?
if grep -q "^14$" "$tmpdir/fout.txt"; then ((++fpass)); else echo "FAIL [follow -f emit]"; ((++ffail)); fi
if [[ $fcode -eq 0 ]]; then ((++fpass)); else echo "FAIL [follow -f exit=$fcode]"; ((++ffail)); fi

# -F: a file that does not exist yet is retried; once it appears, content is
# emitted from the start.
rm -f "$tmpdir/late.txt"
"$CFBOX" tail -F "$tmpdir/late.txt" > "$tmpdir/fout2.txt" 2>/dev/null &
fpid=$!
sleep 2
printf "appeared\n" > "$tmpdir/late.txt"
sleep 2
kill -INT "$fpid" 2>/dev/null
wait "$fpid" 2>/dev/null
if grep -q "appeared" "$tmpdir/fout2.txt"; then ((++fpass)); else echo "FAIL [follow -F retry]"; ((++ffail)); fi

# -f multiple files: a header is emitted on source change
printf "a0\n" > "$tmpdir/a.txt"; printf "b0\n" > "$tmpdir/b.txt"
"$CFBOX" tail -f "$tmpdir/a.txt" "$tmpdir/b.txt" > "$tmpdir/fout3.txt" 2>/dev/null &
fpid=$!
sleep 2
printf "a1\n" >> "$tmpdir/a.txt"
sleep 2
kill -INT "$fpid" 2>/dev/null
wait "$fpid" 2>/dev/null
if grep -q "==>" "$tmpdir/fout3.txt"; then ((++fpass)); else echo "FAIL [follow multi header]"; ((++ffail)); fi

pass=$((pass + fpass)); fail=$((fail + ffail))
echo "tail follow: $fpass passed, $ffail failed"

echo "tail: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
