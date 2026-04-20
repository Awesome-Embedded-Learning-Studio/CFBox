#include <cstdio>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "hostid",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print the numeric identifier for the current host",
    .usage   = "hostid",
    .options = "",
    .extra   = "",
};
} // namespace

auto hostid_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    long id = gethostid();
    std::printf("%08lx\n", id);
    return 0;
}
