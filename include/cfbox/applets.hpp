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
});
