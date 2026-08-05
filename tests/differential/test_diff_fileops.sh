#!/usr/bin/env bash
# Differential tests: cfbox vs busybox on filesystem applets (ls/find/test/
# cp/mv/rm/mkdir). Uses run_diff_fs — per-oracle fixture dir + stdout/exit/
# tree compare, so write ops are covered. See framework.sh and README.md.
set -euo pipefail
source "$(dirname "$0")/framework.sh"

# Fixture: a small tree reused across cases (rebuilt per oracle per case).
setup_basic() {
    local d="$1"
    mkdir -p "$d/sub"
    printf 'hello\n' > "$d/a.txt"
    printf 'world\n' > "$d/b.txt"
    printf 'nested\n' > "$d/sub/c.txt"
}

# --- ls (read-only; stdout compare) ---
run_diff_fs "ls basic"      setup_basic -- ls
run_diff_fs "ls -1"         setup_basic -- ls -1
run_diff_fs "ls -R"         setup_basic -- ls -R
run_diff_fs "ls sub"        setup_basic -- ls sub
run_diff_fs "ls -A"         setup_basic -- ls -A
run_diff_fs "ls missing"    setup_basic -- ls nope

# --- find (read-only) ---
run_diff_fs "find basic"    setup_basic -- find .
run_diff_fs "find -name"    setup_basic -- find . -name '*.txt'
run_diff_fs "find -type f"  setup_basic -- find . -type f
run_diff_fs "find -type d"  setup_basic -- find . -type d

# --- test (exit-code compare; no stdout) ---
run_diff_fs "test -f yes"   setup_basic -- test -f a.txt
run_diff_fs "test -f no"    setup_basic -- test -f nope
run_diff_fs "test -d yes"   setup_basic -- test -d sub
run_diff_fs "test -d no"    setup_basic -- test -d a.txt
run_diff_fs "test -e"       setup_basic -- test -e b.txt

# --- write ops (tree compare catches structural changes) ---
run_diff_fs "cp basic"      setup_basic -- cp a.txt a.copy
run_diff_fs "cp -r"         setup_basic -- cp -r sub sub2
run_diff_fs "mv basic"      setup_basic -- mv a.txt renamed.txt
run_diff_fs "rm basic"      setup_basic -- rm b.txt
run_diff_fs "rm -r"         setup_basic -- rm -r sub
run_diff_fs "mkdir"         setup_basic -- mkdir newdir
run_diff_fs "mkdir -p"      setup_basic -- mkdir -p deep/nested/dir

# --- ls long format (often triages time/owner/column diffs) ---
run_diff_fs "ls -l"         setup_basic -- ls -l
run_diff_fs "ls -la"        setup_basic -- ls -la
run_diff_fs "ls -l sub"     setup_basic -- ls -l sub

# --- ln (structure change) ---
run_diff_fs "ln -s"         setup_basic -- ln -s a.txt alink
run_diff_fs "ln hard"       setup_basic -- ln a.txt ahard

# --- touch / cp variants ---
run_diff_fs "touch new"     setup_basic -- touch created.txt
run_diff_fs "cp -a"         setup_basic -- cp -a a.txt acopy
run_diff_fs "cp overwrite"  setup_basic -- cp a.txt b.txt

# --- chmod (tree's %m mode column catches mode changes) ---
run_diff_fs "chmod oct"     setup_basic -- chmod 600 a.txt
run_diff_fs "chmod sym"     setup_basic -- chmod u+x a.txt
run_diff_fs "chmod -R"      setup_basic -- chmod -R 700 sub

# --- stat (stdout compare; format may triage) ---
run_diff_fs "stat file"     setup_basic -- stat a.txt
run_diff_fs "stat -c fmt"   setup_basic -- stat -c '%n %s %F' a.txt

diff_summary "diff-fileops"
