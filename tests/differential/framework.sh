#!/usr/bin/env bash
# tests/differential/framework.sh — differential testing harness.
#
# Runs the same applet invocation through cfbox and a reference (busybox),
# classifies the result: MATCH (identical stdout + exit), or FAIL (divergence
# worth investigating). KNOWN intentional differences can be filtered with
# skip_if_known, but the default is to surface every divergence so it can be
# triaged.
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

DIFF_PASS=0
DIFF_FAIL=0

diff_check_oracles() {
    [[ -x "$CFBOX" ]]   || { echo "framework: CFBOX not found (build cfbox first)" >&2; exit 2; }
    [[ -x "$BUSYBOX" ]] || { echo "framework: BUSYBOX not found ($BUSYBOX) — run 'make' in competition/busybox" >&2; exit 2; }
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
    if [[ "$cf_out" == "$bb_out" && "$cf_rc" == "$bb_rc" ]]; then
        DIFF_PASS=$((DIFF_PASS + 1))
    else
        echo "FAIL [$desc]  cfbox: $CFBOX $*"
        echo "    cfbox  (rc=$cf_rc): ${cf_out:0:140}"
        echo "    busybox(rc=$bb_rc): ${bb_out:0:140}"
        DIFF_FAIL=$((DIFF_FAIL + 1))
    fi
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
    if [[ "$cf_out" == "$bb_out" && "$cf_rc" == "$bb_rc" ]]; then
        DIFF_PASS=$((DIFF_PASS + 1))
    else
        echo "FAIL [$desc]  stdin fed, args: $*"
        echo "    cfbox  (rc=$cf_rc): ${cf_out:0:140}"
        echo "    busybox(rc=$bb_rc): ${bb_out:0:140}"
        DIFF_FAIL=$((DIFF_FAIL + 1))
    fi
}

diff_summary() {
    local name="${1:-differential}"
    echo "$name: $DIFF_PASS match, $DIFF_FAIL fail"
    [[ $DIFF_FAIL -eq 0 ]]
}

diff_check_oracles
