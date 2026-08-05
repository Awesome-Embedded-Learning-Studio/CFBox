#!/usr/bin/env bash
# tests/differential/framework.sh — differential testing harness.
#
# Runs the same applet invocation through cfbox and a reference (busybox),
# classifies the result against known_diffs:
#   MATCH       — stdout + exit (+ file tree, for fs cases) identical
#   ACCEPTABLE  — registered, intentional diff (format / POSIX-allowed)
#   DEFECT      — registered, known bug (delete the line once fixed)
#   NEW_DIFF    — unregistered divergence → diff_summary exits 1 for triage
#
# Three entry points:
#   run_diff    DESC -- APPLET ARGS...          (args only)
#   run_diff_in DESC INPUT -- APPLET ARGS...    (args + stdin)
#   run_diff_fs DESC SETUP_FN -- APPLET ARGS... (cwd=a fresh fixture dir per
#                oracle; SETUP_FN $dir populates it; compares stdout + exit
#                + sorted file tree, so write ops like cp/mv/rm are covered)
#
# Env:
#   CFBOX    cfbox binary (default: build-size/cfbox or build/cfbox)
#   BUSYBOX  reference oracle (default: competition/busybox/busybox)
#
# Source this file; then call run_diff* and diff_summary.
set -euo pipefail

DIFF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$DIFF_DIR/../.." && pwd)"

CFBOX="${CFBOX:-}"
if [[ -z "$CFBOX" ]]; then
    for c in "$PROJECT_DIR/build-size/cfbox" "$PROJECT_DIR/build/cfbox"; do
        [[ -x "$c" ]] && CFBOX="$c" && break
    done
fi
BUSYBOX="${BUSYBOX:-$PROJECT_DIR/competition/busybox/busybox}"

n_match=0
n_acceptable=0
n_defect=0
n_new=0
new_cases=()

diff_check_oracles() {
    [[ -x "$CFBOX" ]]   || { echo "framework: CFBOX not found (build cfbox first)" >&2; exit 2; }
    [[ -x "$BUSYBOX" ]] || { echo "framework: BUSYBOX not found ($BUSYBOX) — run 'make' in competition/busybox" >&2; exit 2; }
}

# known_diffs: each line "STATUS DESC..." where STATUS ∈ ACCEPTABLE|DEFECT
# (# comments ignored); DESC is the exact first arg passed to run_diff*.
declare -A KNOWN_STATUS
load_known() {
    local f="$DIFF_DIR/known_diffs" st desc
    [[ -r "$f" ]] || return 0
    while read -r st desc; do
        [[ -z "$st" || "$st" == \#* ]] && continue
        KNOWN_STATUS["$desc"]="$st"
    done < "$f"
}
load_known

# _record DESC MATCH CF_DUMP BB_DUMP — classify one case and tally it.
_record() {
    local desc="$1" match="$2" cf_dump="$3" bb_dump="$4"
    local status
    if [[ "$match" == 1 ]]; then
        status=MATCH
    else
        status="${KNOWN_STATUS["$desc"]:-NEW_DIFF}"
    fi
    case "$status" in
        MATCH)      n_match=$((n_match + 1)) ;;
        ACCEPTABLE) n_acceptable=$((n_acceptable + 1)) ;;
        DEFECT)     n_defect=$((n_defect + 1)) ;;
        NEW_DIFF)   n_new=$((n_new + 1)); new_cases+=("$desc") ;;
    esac
    if [[ "$status" != MATCH ]]; then
        printf '  [%s] %s\n' "$status" "$desc"
        printf '    %s\n' "$cf_dump"
        printf '    %s\n' "$bb_dump"
    fi
}

# run_diff DESC -- APPLET ARGS...
run_diff() {
    local desc="$1"; shift
    [[ "${1:-}" == "--" ]] && shift
    local cf_out cf_rc bb_out bb_rc
    set +e
    cf_out=$("$CFBOX" "$@" 2>/dev/null); cf_rc=$?
    bb_out=$("$BUSYBOX" "$@" 2>/dev/null); bb_rc=$?
    set -e
    local match=0
    [[ "$cf_out" == "$bb_out" && "$cf_rc" == "$bb_rc" ]] && match=1
    _record "$desc" "$match" \
        "cfbox  (rc=$cf_rc): ${cf_out:0:120}" \
        "busybox(rc=$bb_rc): ${bb_out:0:120}"
}

# run_diff_in DESC INPUT -- APPLET ARGS...   (INPUT fed on stdin to both)
run_diff_in() {
    local desc="$1"; local input="$2"; shift 2
    [[ "${1:-}" == "--" ]] && shift
    local cf_out cf_rc bb_out bb_rc
    set +e
    cf_out=$(printf '%s' "$input" | "$CFBOX" "$@" 2>/dev/null); cf_rc=$?
    bb_out=$(printf '%s' "$input" | "$BUSYBOX" "$@" 2>/dev/null); bb_rc=$?
    set -e
    local match=0
    [[ "$cf_out" == "$bb_out" && "$cf_rc" == "$bb_rc" ]] && match=1
    _record "$desc" "$match" \
        "cfbox  (rc=$cf_rc): ${cf_out:0:120}" \
        "busybox(rc=$bb_rc): ${bb_out:0:120}"
}

# Snapshot the cwd file tree: type / octal-mode / size / path, sorted.
# Catches mode changes (chmod), size changes (cp -a of different content),
# and structure changes (mkdir/rm/cp). GNU find -printf (Linux CI supports it).
_snap_tree() {
    find . -mindepth 1 -printf '%y%m %s %p\n' 2>/dev/null | sort
}

# run_diff_fs DESC SETUP_FN -- APPLET ARGS...
#   Builds two fresh temp dirs, runs SETUP_FN $dir in each to populate the
#   same fixture, then runs APPLET with cwd=dir for each oracle. Compares
#   stdout + exit code + sorted file tree. Cleans up both dirs.
run_diff_fs() {
    local desc="$1"; local setup="$2"; shift 2
    [[ "${1:-}" == "--" ]] && shift
    local cf_dir bb_dir
    cf_dir=$(mktemp -d)
    bb_dir=$(mktemp -d)
    $setup "$cf_dir"
    $setup "$bb_dir"
    local cf_out cf_rc bb_out bb_rc
    set +e
    cf_out=$(cd "$cf_dir" && "$CFBOX" "$@" 2>/dev/null); cf_rc=$?
    bb_out=$(cd "$bb_dir" && "$BUSYBOX" "$@" 2>/dev/null); bb_rc=$?
    set -e
    local cf_tree bb_tree
    cf_tree=$(cd "$cf_dir" && _snap_tree)
    bb_tree=$(cd "$bb_dir" && _snap_tree)
    rm -rf "$cf_dir" "$bb_dir"
    local match=0
    [[ "$cf_out" == "$bb_out" && "$cf_rc" == "$bb_rc" && "$cf_tree" == "$bb_tree" ]] && match=1
    _record "$desc" "$match" \
        "cfbox  (rc=$cf_rc): ${cf_out:0:60} | tree: ${cf_tree:0:80}" \
        "busybox(rc=$bb_rc): ${bb_out:0:60} | tree: ${bb_tree:0:80}"
}

diff_summary() {
    local name="${1:-differential}"
    echo "$name: MATCH=$n_match ACCEPTABLE=$n_acceptable DEFECT=$n_defect NEW_DIFF=$n_new"
    if (( n_new > 0 )); then
        echo "NEW_DIFF — triage into known_diffs (ACCEPTABLE or DEFECT):" >&2
        printf '  - %s\n' "${new_cases[@]}" >&2
    fi
    [[ $n_new -eq 0 ]]
}

diff_check_oracles
