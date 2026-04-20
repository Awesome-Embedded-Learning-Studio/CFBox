#include <cstdio>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "sync",
    .version = CFBOX_VERSION_STRING,
    .one_line = "synchronize cached writes to persistent storage",
    .usage   = "sync",
    .options = "",
    .extra   = "",
};
} // namespace

auto sync_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    ::sync();
    return 0;
}
