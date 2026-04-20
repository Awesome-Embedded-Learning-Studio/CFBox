#include <cstdio>
#include <pwd.h>
#include <string>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "whoami",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print effective user ID",
    .usage   = "whoami",
    .options = "",
    .extra   = "",
};
} // namespace

auto whoami_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto* pw = getpwuid(geteuid());
    if (pw) {
        std::puts(pw->pw_name);
    } else {
        std::printf("%u\n", static_cast<unsigned>(geteuid()));
    }
    return 0;
}
