#pragma once

#include <cfbox/applet.hpp>

// applet entry points — add declaration here when adding a new applet
extern auto echo_main(int argc, char* argv[]) -> int;
extern auto printf_main(int argc, char* argv[]) -> int;
extern auto cat_main(int argc, char* argv[]) -> int;
extern auto head_main(int argc, char* argv[]) -> int;
extern auto tail_main(int argc, char* argv[]) -> int;
extern auto wc_main(int argc, char* argv[]) -> int;
extern auto sort_main(int argc, char* argv[]) -> int;
extern auto uniq_main(int argc, char* argv[]) -> int;
extern auto mkdir_main(int argc, char* argv[]) -> int;
extern auto rm_main(int argc, char* argv[]) -> int;
extern auto cp_main(int argc, char* argv[]) -> int;
extern auto mv_main(int argc, char* argv[]) -> int;
extern auto ls_main(int argc, char* argv[]) -> int;
extern auto grep_main(int argc, char* argv[]) -> int;
extern auto find_main(int argc, char* argv[]) -> int;
extern auto sed_main(int argc, char* argv[]) -> int;

// registry — one line per applet, easy to generate/extend
constexpr auto APPLET_REGISTRY = std::to_array<cfbox::applet::AppEntry>({
    {"echo",   echo_main,   "display a line of text"},
    {"printf", printf_main, "format and print data"},
    {"cat",    cat_main,    "concatenate files and print"},
    {"head",   head_main,   "output the first part of files"},
    {"tail",   tail_main,   "output the last part of files"},
    {"wc",     wc_main,     "print newline, word, and byte counts"},
    {"sort",   sort_main,   "sort lines of text"},
    {"uniq",   uniq_main,   "report or omit repeated lines"},
    {"mkdir",  mkdir_main,  "create directories"},
    {"rm",     rm_main,     "remove files or directories"},
    {"cp",     cp_main,     "copy files and directories"},
    {"mv",     mv_main,     "move or rename files"},
    {"ls",     ls_main,     "list directory contents"},
    {"grep",   grep_main,   "search patterns in text"},
    {"find",   find_main,   "search for files in directory hierarchy"},
    {"sed",    sed_main,    "stream editor for filtering and transforming text"},
});
