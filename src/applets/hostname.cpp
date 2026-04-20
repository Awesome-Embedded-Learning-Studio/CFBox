#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "hostname",
    .version = CFBOX_VERSION_STRING,
    .one_line = "show or set the system host name",
    .usage   = "hostname [-s]",
    .options = "  -s     short hostname (truncate at first dot)",
    .extra   = "",
};
} // namespace

auto hostname_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0) {
        std::fprintf(stderr, "cfbox hostname: cannot get hostname\n");
        return 1;
    }

    std::string name{buf};
    if (parsed.has('s')) {
        auto dot = name.find('.');
        if (dot != std::string::npos) {
            name.erase(dot);
        }
    }

    std::puts(name.c_str());
    return 0;
}
