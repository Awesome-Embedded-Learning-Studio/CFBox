#include <cstdio>
#include <string>
#include <vector>

#include "awk.hpp"
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "awk",
    .version = CFBOX_VERSION_STRING,
    .one_line = "pattern-directed scanning and processing language",
    .usage   = "awk [-F SEP] [-v VAR=VAL] 'PROGRAM' [FILE]...",
    .options = "  -F SEP  field separator\n"
               "  -v VAR=VAL  variable assignment",
    .extra   = "Supports: BEGIN/END, if/else/while/for/for-in, print/printf,\n"
               "functions, arrays, regex matching, and most built-in functions.",
};
} // namespace

auto awk_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'F', true, "field-separator"},
        cfbox::args::OptSpec{'v', true, "assign"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    cfbox::awk::AwkState state;

    if (auto f = parsed.get_any('F', "field-separator")) {
        state.fs = std::string{*f};
    }
    if (auto v = parsed.get_any('v', "assign")) {
        auto assign = std::string{*v};
        auto eq = assign.find('=');
        if (eq != std::string::npos) {
            state.vars[assign.substr(0, eq)] = assign.substr(eq + 1);
        }
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox awk: missing program\n");
        return 1;
    }

    std::string program{pos[0]};
    std::vector<std::string> files;
    for (std::size_t i = 1; i < pos.size(); ++i) {
        files.emplace_back(pos[i]);
    }

    auto ast = cfbox::awk::awk_parse(program);
    return cfbox::awk::awk_execute(ast, state, files);
}
