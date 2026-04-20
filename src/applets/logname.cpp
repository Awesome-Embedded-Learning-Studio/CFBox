#include <cstdio>
#include <cstdlib>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "logname",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print the user's login name",
    .usage   = "logname",
    .options = "",
    .extra   = "",
};
} // namespace

auto logname_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const char* name = getlogin();
    if (!name) name = std::getenv("LOGNAME");
    if (!name) name = std::getenv("USER");

    if (!name || name[0] == '\0') {
        std::fprintf(stderr, "cfbox logname: no login name\n");
        return 1;
    }

    std::puts(name);
    return 0;
}
