#include <cstdio>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "tty",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print the file name of the terminal connected to stdin",
    .usage   = "tty [-s]",
    .options = "  -s     silent mode (no output, only exit code)",
    .extra   = "",
};
} // namespace

auto tty_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool silent = parsed.has('s');

    if (isatty(STDIN_FILENO)) {
        if (!silent) {
            auto* name = ttyname(STDIN_FILENO);
            if (name) std::puts(name);
        }
        return 0;
    } else {
        if (!silent) {
            std::puts("not a tty");
        }
        return 1;
    }
}
