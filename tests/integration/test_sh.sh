#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

SH="$CFBOX sh"

# ── Basic command execution ──────────────────────────────────────
out=$($SH -c "echo hello")
assert_output "hello" "$out"
((++pass))

# ── Pipeline ─────────────────────────────────────────────────────
out=$($SH -c 'echo hello | wc -l')
assert_output "1" "$out"
((++pass))

out=$($SH -c 'echo aaa | cat')
assert_output "aaa" "$out"
((++pass))

out=$($SH -c 'echo -n hi | cat')
assert_output "hi" "$out"
((++pass))

# ── Variable expansion ───────────────────────────────────────────
out=$($SH -c 'X=hello; echo $X')
assert_output "hello" "$out"
((++pass))

out=$($SH -c 'X=world; echo "hi $X"')
assert_output "hi world" "$out"
((++pass))

# ── For loop ─────────────────────────────────────────────────────
out=$($SH -c 'for i in 1 2 3; do echo $i; done')
expected=$'1\n2\n3'
assert_output "$expected" "$out"
((++pass))

out=$($SH -c 'for x in a b c; do echo -$x-; done')
expected=$'-a-\n-b-\n-c-'
assert_output "$expected" "$out"
((++pass))

# ── If/then/else ─────────────────────────────────────────────────
out=$($SH -c 'if true; then echo yes; fi')
assert_output "yes" "$out"
((++pass))

out=$($SH -c 'if false; then echo yes; else echo no; fi')
assert_output "no" "$out"
((++pass))

out=$($SH -c 'if false; then echo a; elif true; then echo b; else echo c; fi')
assert_output "b" "$out"
((++pass))

# ── While loop ───────────────────────────────────────────────────
out=$($SH -c 'i=0; while [ $i -lt 3 ]; do echo $i; i=3; done')
assert_output "0" "$out"
((++pass))

# ── && and || ────────────────────────────────────────────────────
out=$($SH -c 'true && echo pass || echo fail')
assert_output "pass" "$out"
((++pass))

out=$($SH -c 'false && echo pass || echo fail')
assert_output "fail" "$out"
((++pass))

# ── Redirections ─────────────────────────────────────────────────
$SH -c "echo redir_out > $tmpdir/r.txt"
out=$(cat "$tmpdir/r.txt")
assert_output "redir_out" "$out"
((++pass))

out=$($SH -c "echo hi > $tmpdir/r2.txt; cat < $tmpdir/r2.txt")
assert_output "hi" "$out"
((++pass))

out=$($SH -c "echo a > $tmpdir/r3.txt; echo b >> $tmpdir/r3.txt; cat $tmpdir/r3.txt")
expected=$'a\nb'
assert_output "$expected" "$out"
((++pass))

# ── Command substitution ─────────────────────────────────────────
out=$($SH -c 'echo $(echo nested)')
assert_output "nested" "$out"
((++pass))

# ── Arithmetic $((expr)) ─────────────────────────────────────────
out=$($SH -c 'echo $((1 + 2 * 3))')
assert_output "7" "$out"
((++pass))

out=$($SH -c 'echo $((10 - 4))')
assert_output "6" "$out"
((++pass))

out=$($SH -c 'i=5; echo $((i + 1))')
assert_output "6" "$out"
((++pass))

out=$($SH -c 'i=0; while [ $i -lt 3 ]; do echo $i; i=$((i+1)); done')
expected=$'0\n1\n2'
assert_output "$expected" "$out"
((++pass))

out=$($SH -c 'echo $(( (2 + 3) * 4 ))')
assert_output "20" "$out"
((++pass))

# ── case statement ───────────────────────────────────────────────
out=$($SH -c 'case start in start) echo go;; stop) echo halt;; esac')
assert_output "go" "$out"
((++pass))

out=$($SH -c 'case x in a|b) echo ab;; *) echo other;; esac')
assert_output "other" "$out"
((++pass))

out=$($SH -c 'case foo in f*) echo begins_f;; *) echo no;; esac')
assert_output "begins_f" "$out"
((++pass))

out=$($SH -c 'for w in 1 2 3; do case $w in 1) echo one;; 2) echo two;; *) echo many;; esac; done')
expected=$'one\ntwo\nmany'
assert_output "$expected" "$out"
((++pass))

# ── Functions, return, local ─────────────────────────────────────
out=$($SH -c 'greet() { echo hi; }; greet')
assert_output "hi" "$out"
((++pass))

out=$($SH -c 'add() { echo $(($1 + $2)); }; add 3 4')
assert_output "7" "$out"
((++pass))

set +e
$SH -c 'f() { return 42; }; f'
rc=$?
set -e
[[ $rc -eq 42 ]] && ((++pass)) || { echo "FAIL [return status]: $rc"; ((++fail)); }

out=$($SH -c 'g() { local x=L; echo $x; }; x=G; g; echo $x')
expected=$'L\nG'
assert_output "$expected" "$out"
((++pass))

# ── Here-doc ────────────────────────────────────────────────────
out=$($SH -c 'cat <<EOF
hello
world
EOF')
expected=$'hello\nworld'
assert_output "$expected" "$out"
((++pass))

out=$($SH -c 'V=42; cat <<EOF
val=$V
EOF')
assert_output "val=42" "$out"
((++pass))

out=$($SH -c 'cat <<EOF
$((3+4))
EOF')
assert_output "7" "$out"
((++pass))

# ── Advanced ${} parameter expansion ─────────────────────────────
out=$($SH -c 'X=hello; echo ${#X}')
assert_output "5" "$out"
((++pass))

out=$($SH -c 'F=a.txt; echo ${F%.txt}')
assert_output "a" "$out"
((++pass))

out=$($SH -c 'F=pre_data; echo ${F#pre_}')
assert_output "data" "$out"
((++pass))

out=$($SH -c 'P=/a/b/c.txt; echo ${P##*/}')
assert_output "c.txt" "$out"
((++pass))

out=$($SH -c 'echo ${MISS:-fallback}')
assert_output "fallback" "$out"
((++pass))

# ── break / continue / break N ──────────────────────────────────
out=$($SH -c 'for i in 1 2 3; do [ $i = 2 ] && break; echo $i; done')
assert_output "1" "$out"
((++pass))

out=$($SH -c 'for i in 1 2 3; do [ $i = 2 ] && continue; echo $i; done')
expected=$'1\n3'
assert_output "$expected" "$out"
((++pass))

out=$($SH -c 'for i in 1 2; do for j in a b; do [ $j = b ] && break 2; echo $j; done; echo X; done')
assert_output "a" "$out"
((++pass))

# ── Subshell ─────────────────────────────────────────────────────
out=$($SH -c '(echo sub1; echo sub2)')
expected=$'sub1\nsub2'
assert_output "$expected" "$out"
((++pass))

# ── Brace group ──────────────────────────────────────────────────
out=$($SH -c '{ echo b1; echo b2; }')
expected=$'b1\nb2'
assert_output "$expected" "$out"
((++pass))

# ── Builtin cd ───────────────────────────────────────────────────
out=$($SH -c "cd /tmp; pwd")
assert_output "/tmp" "$out"
((++pass))

# ── Builtin export ───────────────────────────────────────────────
out=$($SH -c 'export MYVAR=42; echo $MYVAR')
assert_output "42" "$out"
((++pass))

# ── Builtin exit ─────────────────────────────────────────────────
set +e
$SH -c 'exit 42'
rc=$?
set -e
[[ $rc -eq 42 ]]
((++pass))

# ── Builtin shift ────────────────────────────────────────────────
out=$($SH -c 'set -- a b c; shift; echo $1')
assert_output "b" "$out"
((++pass))

# ── Tilde expansion ──────────────────────────────────────────────
out=$($SH -c 'echo ~' | head -1)
[[ -n "$out" ]]
((++pass))

# ── Special parameters ───────────────────────────────────────────
out=$($SH -c 'echo $#' sh_name a b c)
assert_output "3" "$out"
((++pass))

# ── --help / --version ───────────────────────────────────────────
assert_exit 0 sh --help
((++pass))

assert_exit 0 sh --version
((++pass))

echo "sh: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
