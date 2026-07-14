#!/usr/bin/env bash
# tests/posix/coverage.sh — POSIX.1-2017 XCU conformance coverage for cfbox.
#
# Two dimensions:
#   1. utility  — POSIX mandatory utilities cfbox ships as standalone applets
#      (static: compared against `cfbox --list`)
#   2. builtin  — POSIX mandatory shell builtins cfbox sh actually supports
#      (dynamic: behavior probe via `cfbox sh -c`, NOT declaration)
#
# CI gate: coverage must not regress below tests/posix/baseline — the
# COVERAGE.md "only up, never down" spirit. Exits non-zero on regression.
# When coverage rises, run with --update-baseline to lock the new floor.
#
# Usage: tests/posix/coverage.sh            # measure + gate
#        tests/posix/coverage.sh --update-baseline
#        CFBOX=./build/cfbox tests/posix/coverage.sh
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
project_dir="$(cd "$script_dir/../.." && pwd)"
baseline_file="$script_dir/baseline"

# locate cfbox binary (CI builds into build/, local size-opt into build-size/)
CFBOX="${CFBOX:-}"
if [[ -z "$CFBOX" ]]; then
    for cand in "$project_dir/build/cfbox" "$project_dir/build-size/cfbox"; do
        [[ -x "$cand" ]] && CFBOX="$cand" && break
    done
fi
[[ -x "$CFBOX" ]] || { echo "ERROR: no cfbox binary found (build it first)"; exit 2; }

# POSIX.1-2017 XCU mandatory utilities — standalone, with the shell builtins
# removed (cd/command/type/ulimit/... are measured in dimension 2, not here).
POSIX_UTILITIES=(basename cat chgrp chmod chown cksum cmp comm cp csplit cut
    date dd df diff dirname du echo env expand expr false file find fold getconf
    grep head id kill link ln locale logger logname ls mkdir mkfifo mv nice nl
    nohup od paste patch pathchk pr printf pwd renice rm rmdir sed sleep
    sort split strings strip stty tail tee test touch tput tr true tsort tty
    uname unexpand uniq unlink uudecode uuencode wc what xargs)

# POSIX.1-2017 mandatory shell builtins, each with a minimal behavior probe.
# A builtin is "supported" if the probe does NOT print "<name>: command not found".
declare -A BUILTIN_PROBE=(
    [cd]='cd /tmp && pwd'
    [command]='command echo ok'
    [eval]="eval 'echo evaled'"
    [exec]='exec echo done'
    [exit]='exit 0'
    [export]='export X=hi; echo $X'
    [getopts]='while getopts ab: opt -a; do echo $opt; break; done'
    [hash]='hash; echo ok'
    [read]='echo hi | (read x; echo $x)'
    [return]='f(){ return 3; }; f; echo $?'
    [set]='set -- a b; echo $#'
    [shift]='set -- a b c; shift; echo $1'
    [times]='times'
    [trap]="trap 'echo trapped' EXIT"
    [type]='type sh'
    [ulimit]='ulimit'
    [umask]='umask 022; umask'
    [unalias]='unalias cfbox_no_such_alias'
    [unset]='x=1; unset x; echo "[${x:-empty}]"'
    [wait]='true & wait; echo done'
)

# --- dimension 1: utility coverage (static) -----------------------------------
applet_set=" $("$CFBOX" --list | awk '{print $1}' | tr '\n' ' ') "
util_covered=0
util_missing=()
for u in "${POSIX_UTILITIES[@]}"; do
    if [[ "$applet_set" == *" $u "* ]]; then
        util_covered=$((util_covered + 1))
    else
        util_missing+=("$u")
    fi
done
util_total=${#POSIX_UTILITIES[@]}

# --- dimension 2: builtin coverage (behavior probe) ---------------------------
builtin_covered=0
builtin_missing=()
for b in "${!BUILTIN_PROBE[@]}"; do
    out=$("$CFBOX" sh -c "${BUILTIN_PROBE[$b]}" 2>&1 || true)
    if echo "$out" | grep -qE "(^|[^a-z])$b: command not found"; then
        builtin_missing+=("$b")
    else
        builtin_covered=$((builtin_covered + 1))
    fi
done
builtin_total=${#BUILTIN_PROBE[@]}

# --- report -------------------------------------------------------------------
util_pct=$((util_covered * 100 / util_total))
builtin_pct=$((builtin_covered * 100 / builtin_total))
total_covered=$((util_covered + builtin_covered))
total_total=$((util_total + builtin_total))
total_pct=$((total_covered * 100 / total_total))

echo "=== POSIX.1-2017 XCU conformance coverage ==="
echo "cfbox: $CFBOX"
echo ""
printf "  utility (applet):  %d/%d (%d%%)\n" "$util_covered" "$util_total" "$util_pct"
printf "  builtin (sh):      %d/%d (%d%%)\n" "$builtin_covered" "$builtin_total" "$builtin_pct"
printf "  total:             %d/%d (%d%%)\n" "$total_covered" "$total_total" "$total_pct"
echo ""
if [[ ${#util_missing[@]} -gt 0 ]]; then
    echo "utility missing (${#util_missing[@]}): ${util_missing[*]}"
fi
if [[ ${#builtin_missing[@]} -gt 0 ]]; then
    echo "builtin missing (${#builtin_missing[@]}): ${builtin_missing[*]}"
fi

# --- baseline gate (only-up) --------------------------------------------------
if [[ "${1:-}" == "--update-baseline" ]]; then
    printf 'utility %d\nbuiltin %d\n' "$util_covered" "$builtin_covered" > "$baseline_file"
    echo ""
    echo "baseline written: utility=$util_covered builtin=$builtin_covered → $baseline_file"
    exit 0
fi

if [[ ! -f "$baseline_file" ]]; then
    echo ""
    echo "WARNING: no baseline file ($baseline_file). Run '$0 --update-baseline' to set the floor."
    exit 0
fi

base_util=$(awk '/^utility /{print $2}' "$baseline_file")
base_builtin=$(awk '/^builtin /{print $2}' "$baseline_file")
echo ""
echo "=== baseline gate (only-up; floor: utility=$base_util builtin=$base_builtin) ==="
fail=0
if (( util_covered < base_util )); then
    echo "FAIL: utility $util_covered < $base_util (regression)"; fail=1
fi
if (( builtin_covered < base_builtin )); then
    echo "FAIL: builtin $builtin_covered < $base_builtin (regression)"; fail=1
fi
if [[ $fail -eq 0 ]]; then
    if (( util_covered > base_util || builtin_covered > base_builtin )); then
        echo "PASS — coverage rose above baseline; run with --update-baseline to lock it in"
    else
        echo "PASS"
    fi
fi
exit $fail
