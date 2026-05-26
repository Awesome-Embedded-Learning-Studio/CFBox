#include <cstdio>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/terminal.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "clear",
    .version = CFBOX_VERSION_STRING,
    .one_line = "clear the terminal screen",
    .usage   = "clear",
    .options = "",
    .extra   = "",
};
} // namespace

auto clear_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    cfbox::terminal::clear_screen();
    std::fflush(stdout);
    return 0;
}
