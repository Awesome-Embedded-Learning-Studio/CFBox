#pragma once

#include <cfbox/applet.hpp>
#include <cfbox/applet_config.hpp>

// applet entry points — conditionally declared based on config
#if CFBOX_ENABLE_ECHO
extern auto echo_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_PRINTF
extern auto printf_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_CAT
extern auto cat_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_HEAD
extern auto head_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TAIL
extern auto tail_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_WC
extern auto wc_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SORT
extern auto sort_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_UNIQ
extern auto uniq_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_MKDIR
extern auto mkdir_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_RM
extern auto rm_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_CP
extern auto cp_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_MV
extern auto mv_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_LS
extern auto ls_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_GREP
extern auto grep_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_FIND
extern auto find_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SED
extern auto sed_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_INIT
extern auto init_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TRUE
extern auto true_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_FALSE
extern auto false_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_YES
extern auto yes_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_PWD
extern auto pwd_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_BASENAME
extern auto basename_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_DIRNAME
extern auto dirname_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_UNAME
extern auto uname_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_NPROC
extern auto nproc_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_LINK
extern auto link_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_HOSTNAME
extern auto hostname_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_LOGNAME
extern auto logname_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_WHOAMI
extern auto whoami_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TTY
extern auto tty_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SLEEP
extern auto sleep_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_ID
extern auto id_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TEST
extern auto test_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SH
extern auto sh_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_PRINTENV
extern auto printenv_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_HOSTID
extern auto hostid_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SYNC
extern auto sync_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_USLEEP
extern auto usleep_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_RMDIR
extern auto rmdir_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_UNLINK
extern auto unlink_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_WHO
extern auto who_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_ENV
extern auto env_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_READLINK
extern auto readlink_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_REALPATH
extern auto realpath_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TOUCH
extern auto touch_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TRUNCATE
extern auto truncate_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_STAT
extern auto stat_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_INSTALL
extern auto install_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_MKTEMP
extern auto mktemp_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_LN
extern auto ln_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_MKFIFO
extern auto mkfifo_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_MKNOD
extern auto mknod_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_DU
extern auto du_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SEQ
extern auto seq_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TEE
extern auto tee_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TAC
extern auto tac_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_FOLD
extern auto fold_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_EXPAND
extern auto expand_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_CUT
extern auto cut_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_PASTE
extern auto paste_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_NL
extern auto nl_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_COMM
extern auto comm_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TR
extern auto tr_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_CKSUM
extern auto cksum_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_MD5SUM
extern auto md5sum_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SUM
extern auto sum_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_DATE
extern auto date_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_OD
extern auto od_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SPLIT
extern auto split_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_SHUF
extern auto shuf_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_FACTOR
extern auto factor_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TIMEOUT
extern auto timeout_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_NICE
extern auto nice_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_NOHUP
extern auto nohup_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_DF
extern auto df_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_EXPR
extern auto expr_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_TSORT
extern auto tsort_main(int argc, char* argv[]) -> int;
#endif
#if CFBOX_ENABLE_XARGS
extern auto xargs_main(int argc, char* argv[]) -> int;
#endif

// registry — one line per applet, conditionally compiled
constexpr auto APPLET_REGISTRY = std::to_array<cfbox::applet::AppEntry>({
#if CFBOX_ENABLE_ECHO
    {"echo",   echo_main,   "display a line of text"},
#endif
#if CFBOX_ENABLE_PRINTF
    {"printf", printf_main, "format and print data"},
#endif
#if CFBOX_ENABLE_CAT
    {"cat",    cat_main,    "concatenate files and print"},
#endif
#if CFBOX_ENABLE_HEAD
    {"head",   head_main,   "output the first part of files"},
#endif
#if CFBOX_ENABLE_TAIL
    {"tail",   tail_main,   "output the last part of files"},
#endif
#if CFBOX_ENABLE_WC
    {"wc",     wc_main,     "print newline, word, and byte counts"},
#endif
#if CFBOX_ENABLE_SORT
    {"sort",   sort_main,   "sort lines of text"},
#endif
#if CFBOX_ENABLE_UNIQ
    {"uniq",   uniq_main,   "report or omit repeated lines"},
#endif
#if CFBOX_ENABLE_MKDIR
    {"mkdir",  mkdir_main,  "create directories"},
#endif
#if CFBOX_ENABLE_RM
    {"rm",     rm_main,     "remove files or directories"},
#endif
#if CFBOX_ENABLE_CP
    {"cp",     cp_main,     "copy files and directories"},
#endif
#if CFBOX_ENABLE_MV
    {"mv",     mv_main,     "move or rename files"},
#endif
#if CFBOX_ENABLE_LS
    {"ls",     ls_main,     "list directory contents"},
#endif
#if CFBOX_ENABLE_GREP
    {"grep",   grep_main,   "search patterns in text"},
#endif
#if CFBOX_ENABLE_FIND
    {"find",   find_main,   "search for files in directory hierarchy"},
#endif
#if CFBOX_ENABLE_SED
    {"sed",    sed_main,    "stream editor for filtering and transforming text"},
#endif
#if CFBOX_ENABLE_INIT
    {"init",   init_main,   "system init for boot testing (PID 1)"},
#endif
#if CFBOX_ENABLE_TRUE
    {"true",   true_main,   "do nothing, exit with status 0"},
#endif
#if CFBOX_ENABLE_FALSE
    {"false",  false_main,  "do nothing, exit with status 1"},
#endif
#if CFBOX_ENABLE_YES
    {"yes",    yes_main,    "output a string repeatedly until killed"},
#endif
#if CFBOX_ENABLE_PWD
    {"pwd",    pwd_main,    "print working directory"},
#endif
#if CFBOX_ENABLE_BASENAME
    {"basename", basename_main, "strip directory and suffix from file names"},
#endif
#if CFBOX_ENABLE_DIRNAME
    {"dirname",  dirname_main,  "strip last component from file name"},
#endif
#if CFBOX_ENABLE_UNAME
    {"uname",  uname_main,  "print system information"},
#endif
#if CFBOX_ENABLE_NPROC
    {"nproc",  nproc_main,  "print number of available processors"},
#endif
#if CFBOX_ENABLE_LINK
    {"link",   link_main,   "create a hard link"},
#endif
#if CFBOX_ENABLE_HOSTNAME
    {"hostname", hostname_main, "show or set the system host name"},
#endif
#if CFBOX_ENABLE_LOGNAME
    {"logname", logname_main,  "print the user's login name"},
#endif
#if CFBOX_ENABLE_WHOAMI
    {"whoami", whoami_main, "print effective user ID"},
#endif
#if CFBOX_ENABLE_TTY
    {"tty",    tty_main,    "print the file name of the terminal connected to stdin"},
#endif
#if CFBOX_ENABLE_SLEEP
    {"sleep",  sleep_main,  "delay for a specified amount of time"},
#endif
#if CFBOX_ENABLE_ID
    {"id",     id_main,     "print real and effective user and group IDs"},
#endif
#if CFBOX_ENABLE_TEST
    {"test",   test_main,   "evaluate conditional expression"},
    {"[",      test_main,   "evaluate conditional expression"},
#endif
#if CFBOX_ENABLE_SH
    {"sh",     sh_main,     "POSIX shell command interpreter"},
#endif
#if CFBOX_ENABLE_PRINTENV
    {"printenv", printenv_main, "print all or part of environment"},
#endif
#if CFBOX_ENABLE_HOSTID
    {"hostid",  hostid_main,  "print the numeric identifier for the current host"},
#endif
#if CFBOX_ENABLE_SYNC
    {"sync",    sync_main,    "synchronize cached writes to persistent storage"},
#endif
#if CFBOX_ENABLE_USLEEP
    {"usleep",  usleep_main,  "sleep for a specified number of microseconds"},
#endif
#if CFBOX_ENABLE_RMDIR
    {"rmdir",   rmdir_main,   "remove empty directories"},
#endif
#if CFBOX_ENABLE_UNLINK
    {"unlink",  unlink_main,  "call the unlink function to remove a file"},
#endif
#if CFBOX_ENABLE_WHO
    {"who",     who_main,     "show who is logged on"},
#endif
#if CFBOX_ENABLE_ENV
    {"env",     env_main,     "run a program in a modified environment"},
#endif
#if CFBOX_ENABLE_READLINK
    {"readlink", readlink_main, "print the value of a symbolic link"},
#endif
#if CFBOX_ENABLE_REALPATH
    {"realpath", realpath_main, "print the resolved path"},
#endif
#if CFBOX_ENABLE_TOUCH
    {"touch",    touch_main,    "change file timestamps"},
#endif
#if CFBOX_ENABLE_TRUNCATE
    {"truncate", truncate_main, "shrink or extend the size of a file"},
#endif
#if CFBOX_ENABLE_STAT
    {"stat",     stat_main,     "display file or file system status"},
#endif
#if CFBOX_ENABLE_INSTALL
    {"install",  install_main,  "copy files and set attributes"},
#endif
#if CFBOX_ENABLE_MKTEMP
    {"mktemp",   mktemp_main,   "create a temporary file or directory"},
#endif
#if CFBOX_ENABLE_LN
    {"ln",       ln_main,       "make links between files"},
#endif
#if CFBOX_ENABLE_MKFIFO
    {"mkfifo",   mkfifo_main,   "make FIFOs (named pipes)"},
#endif
#if CFBOX_ENABLE_MKNOD
    {"mknod",    mknod_main,    "make block or character special files"},
#endif
#if CFBOX_ENABLE_DU
    {"du",       du_main,       "estimate file space usage"},
#endif
#if CFBOX_ENABLE_SEQ
    {"seq",      seq_main,      "print a sequence of numbers"},
#endif
#if CFBOX_ENABLE_TEE
    {"tee",      tee_main,      "read from stdin and write to stdout and files"},
#endif
#if CFBOX_ENABLE_TAC
    {"tac",      tac_main,      "concatenate and print files in reverse"},
#endif
#if CFBOX_ENABLE_FOLD
    {"fold",     fold_main,     "wrap each input line to fit in specified width"},
#endif
#if CFBOX_ENABLE_EXPAND
    {"expand",   expand_main,   "convert tabs to spaces"},
#endif
#if CFBOX_ENABLE_CUT
    {"cut",      cut_main,      "remove sections from each line of files"},
#endif
#if CFBOX_ENABLE_PASTE
    {"paste",    paste_main,    "merge lines of files"},
#endif
#if CFBOX_ENABLE_NL
    {"nl",       nl_main,       "number lines of files"},
#endif
#if CFBOX_ENABLE_COMM
    {"comm",     comm_main,     "compare two sorted files line by line"},
#endif
#if CFBOX_ENABLE_TR
    {"tr",       tr_main,       "translate, squeeze, and/or delete characters"},
#endif
#if CFBOX_ENABLE_CKSUM
    {"cksum",    cksum_main,    "checksum and count the bytes in a file"},
#endif
#if CFBOX_ENABLE_MD5SUM
    {"md5sum",   md5sum_main,   "compute and check MD5 message digest"},
#endif
#if CFBOX_ENABLE_SUM
    {"sum",      sum_main,      "checksum and count the blocks in a file"},
#endif
#if CFBOX_ENABLE_DATE
    {"date",     date_main,     "print or set the system date and time"},
#endif
#if CFBOX_ENABLE_OD
    {"od",       od_main,       "dump files in octal and other formats"},
#endif
#if CFBOX_ENABLE_SPLIT
    {"split",    split_main,    "split a file into pieces"},
#endif
#if CFBOX_ENABLE_SHUF
    {"shuf",     shuf_main,     "generate random permutations"},
#endif
#if CFBOX_ENABLE_FACTOR
    {"factor",   factor_main,   "print the prime factors of numbers"},
#endif
#if CFBOX_ENABLE_TIMEOUT
    {"timeout",  timeout_main,  "run a command with a time limit"},
#endif
#if CFBOX_ENABLE_NICE
    {"nice",     nice_main,     "run a program with modified scheduling priority"},
#endif
#if CFBOX_ENABLE_NOHUP
    {"nohup",    nohup_main,    "run a command immune to hangups"},
#endif
#if CFBOX_ENABLE_DF
    {"df",       df_main,       "report file system disk space usage"},
#endif
#if CFBOX_ENABLE_EXPR
    {"expr",     expr_main,     "evaluate expressions"},
#endif
#if CFBOX_ENABLE_TSORT
    {"tsort",    tsort_main,    "perform topological sort"},
#endif
#if CFBOX_ENABLE_XARGS
    {"xargs",    xargs_main,    "build and execute command lines from stdin"},
#endif
});
