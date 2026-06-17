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

# --- follow (-f/-F): background process + appended data ----------------
# Each case spawns tail, appends, waits one poll interval, then stops it.
# stop_follow sends SIGINT and falls back to SIGKILL after a watchdog timeout:
# qemu-user does NOT reliably forward SIGINT to the emulated cfbox, so without
# the SIGKILL fallback the suite would hang under QEMU CI. We assert output
# correctness only — exit code is environment-dependent (clean 0 on native,
# killed under QEMU) and is not a pass/fail signal.
fpass=0 ffail=0
stop_follow() {
    local pid=$1
    ( sleep 6; kill -KILL "$pid" 2>/dev/null ) & local wd=$!
    kill -INT "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null || true
    kill "$wd" 2>/dev/null; wait "$wd" 2>/dev/null 2>&1 || true
}

# -f: appended data is emitted
"$CFBOX" tail -f "$tmpdir/nums.txt" > "$tmpdir/fout.txt" 2>/dev/null &
fpid=$!
sleep 2; printf "13\n14\n" >> "$tmpdir/nums.txt"; sleep 2
stop_follow "$fpid"
if grep -q "^14$" "$tmpdir/fout.txt"; then ((++fpass)); else echo "FAIL [follow -f emit]"; ((++ffail)); fi

# -F: a file that does not exist yet is retried; once it appears, content is
# emitted from the start.
rm -f "$tmpdir/late.txt"
"$CFBOX" tail -F "$tmpdir/late.txt" > "$tmpdir/fout2.txt" 2>/dev/null &
fpid=$!
sleep 2; printf "appeared\n" > "$tmpdir/late.txt"; sleep 2
stop_follow "$fpid"
if grep -q "appeared" "$tmpdir/fout2.txt"; then ((++fpass)); else echo "FAIL [follow -F retry]"; ((++ffail)); fi

# -f multiple files: a header is emitted on source change
printf "a0\n" > "$tmpdir/a.txt"; printf "b0\n" > "$tmpdir/b.txt"
"$CFBOX" tail -f "$tmpdir/a.txt" "$tmpdir/b.txt" > "$tmpdir/fout3.txt" 2>/dev/null &
fpid=$!
sleep 2; printf "a1\n" >> "$tmpdir/a.txt"; sleep 2
stop_follow "$fpid"
if grep -q "==>" "$tmpdir/fout3.txt"; then ((++fpass)); else echo "FAIL [follow multi header]"; ((++ffail)); fi

pass=$((pass + fpass)); fail=$((fail + ffail))
echo "tail follow: $fpass passed, $ffail failed"

echo "tail: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
