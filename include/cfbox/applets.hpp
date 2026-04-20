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
});
