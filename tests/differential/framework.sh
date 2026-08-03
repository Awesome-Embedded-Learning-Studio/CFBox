#!/usr/bin/env bash
# tests/differential/framework.sh — differential testing harness.
#
# Runs the same applet invocation through cfbox and a reference (busybox),
# classifies the result against known_diffs:
#   MATCH       — stdout + exit identical
#   ACCEPTABLE  — registered, intentional diff (format / POSIX-allowed)
#   DEFECT      — registered, known bug (delete the line once fixed)
#   NEW_DIFF    — unregistered divergence → diff_summary exits 1 for triage
#
# Env:
#   CFBOX    cfbox binary (default: build-size/cfbox or build/cfbox)
#   BUSYBOX  reference oracle (default: competition/busybox/busybox)
#
# Source this file; then call run_diff / run_diff_in and diff_summary.
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
# (# comments ignored); DESC is the exact first arg passed to run_diff/run_diff_in.
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

# _classify DESC CF_OUT CF_RC BB_OUT BB_RC
_classify() {
    local desc="$1" cf_out="$2" cf_rc="$3" bb_out="$4" bb_rc="$5"
    local status
    if [[ "$cf_out" == "$bb_out" && "$cf_rc" == "$bb_rc" ]]; then
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
        printf '    cfbox  (rc=%s): %s\n' "$cf_rc" "${cf_out:0:120}"
        printf '    busybox(rc=%s): %s\n' "$bb_rc" "${bb_out:0:120}"
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
    _classify "$desc" "$cf_out" "$cf_rc" "$bb_out" "$bb_rc"
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
    _classify "$desc" "$cf_out" "$cf_rc" "$bb_out" "$bb_rc"
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
