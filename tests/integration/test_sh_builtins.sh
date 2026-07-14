#!/usr/bin/env bash
# POSIX mandatory shell builtins coverage (cfbox sh).
set -euo pipefail
source "$(dirname "$0")/helpers.sh"

pass=0 fail=0

ck() { # name expected command  — exact match of `cfbox sh -c command` stdout(+stderr)
    local name="$1" exp="$2" cmd="$3" got
    got=$("$CFBOX" sh -c "$cmd" 2>&1)
    if [[ "$got" == "$exp" ]]; then ((++pass)); else
        echo "FAIL [$name]: want=$(printf '%q' "$exp") got=$(printf '%q' "$got")"; ((++fail))
    fi
}
re() { # name pattern command  — regex match
    local name="$1" pat="$2" cmd="$3" got
    got=$("$CFBOX" sh -c "$cmd" 2>&1)
    if [[ "$got" =~ $pat ]]; then ((++pass)); else
        echo "FAIL [$name]: want=~$pat got=$(printf '%q' "$got")"; ((++fail))
    fi
}

ck "type builtin"      "echo is a shell builtin" 'type echo'
ck "command -v echo"   "echo"                    'command -v echo'
ck "exec replaces"     "EXECED"                  'exec echo EXECED'
ck "umask set+read"    "077"                     'umask 077; umask'
ck "hash success"      "0"                       'hash; echo $?'
ck "unalias success"   "0"                       'unalias nope; echo $?'
ck "getopts"           "a"                       'while getopts ab: opt -a; do echo $opt; break; done'

re "umask read"        '^[0-7]{3}$'              'umask'
re "ulimit -n"         '^[0-9]+$'                'ulimit -n'
re "times"             'm[0-9]+s'                'times'
ck "wait is builtin"   "wait is a shell builtin" 'type wait'   # registered (job-wait needs & job machinery, separate work)

echo "sh_builtins: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
