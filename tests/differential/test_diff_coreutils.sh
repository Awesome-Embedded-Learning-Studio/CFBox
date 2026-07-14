#!/usr/bin/env bash
# Differential tests: cfbox vs busybox on core text-processing applets.
set -euo pipefail
source "$(dirname "$0")/framework.sh"

# --- echo ---
run_diff "echo basic"       -- echo hello world
run_diff "echo -n"          -- echo -n nonewline
run_diff "echo empty"       -- echo
run_diff "echo flags"       -- echo -e 'a\tb'

# --- cat ---
run_diff_in "cat stdin"     "line1
line2
line3" -- cat
run_diff_in "cat -n"        "a
b" -- cat -n

# --- wc ---
run_diff_in "wc -l"         "a
b
c" -- wc -l
run_diff_in "wc -w"         "one two three" -- wc -w
run_diff_in "wc -c"         "abcdef" -- wc -c

# --- head / tail ---
run_diff_in "head -n1"      "a
b
c" -- head -n1
run_diff_in "head -n2"      "1
2
3" -- head -n2
run_diff_in "tail -n1"      "a
b
c" -- tail -n1

# --- sort ---
run_diff_in "sort alpha"    "c
a
b" -- sort
run_diff_in "sort -n"       "10
2
1" -- sort -n
run_diff_in "sort -r"       "a
b
c" -- sort -r
run_diff_in "sort -u"       "a
a
b
b" -- sort -u

# --- uniq ---
run_diff_in "uniq basic"    "a
a
b
a" -- uniq
run_diff_in "uniq -c"       "x
x
y" -- uniq -c

# --- rev / basename / dirname ---
run_diff_in "rev"           "hello" -- rev
run_diff     "basename"     -- basename /a/b/c.txt
run_diff     "basename suf" -- basename /a/b/c.txt .txt
run_diff     "dirname"      -- dirname /a/b/c

# --- tr ---
run_diff_in "tr a-z A-Z"    "hello" -- tr a-z A-Z
run_diff_in "tr -d"         "hello" -- tr -d l

# --- cut ---
run_diff_in "cut -f2"       "a:b:c
1:2:3" -- cut -d: -f2
run_diff_in "cut -c1-3"     "abcdef
ghijkl" -- cut -c1-3

# --- seq / fold / expand ---
run_diff     "seq 1 5"      -- seq 1 5
run_diff     "seq -w 1 3"   -- seq -w 1 3
run_diff_in "fold -w3"      "abcdef" -- fold -w3
run_diff_in "expand"        "a	b" -- expand

# --- printf / true / false ---
run_diff     "printf str"   -- printf '%s\n' hello
run_diff     "true rc"      -- true
run_diff     "false rc"     -- false

diff_summary "diff-coreutils"
