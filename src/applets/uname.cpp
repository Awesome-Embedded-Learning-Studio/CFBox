#include <cstdio>
#include <cstring>
#include <string>
#include <sys/utsname.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "uname",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print system information",
    .usage   = "uname [-snrvma]",
    .options = "  -s     kernel name (default)\n"
               "  -n     nodename\n"
               "  -r     kernel release\n"
               "  -v     kernel version\n"
               "  -m     machine hardware name\n"
               "  -a     print all information",
    .extra   = "",
};
} // namespace

auto uname_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false},
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'r', false},
        cfbox::args::OptSpec{'v', false},
        cfbox::args::OptSpec{'m', false},
        cfbox::args::OptSpec{'a', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    struct utsname info {};
    if (::uname(&info) != 0) {
        std::fprintf(stderr, "cfbox uname: failed to get system information\n");
        return 1;
    }

    bool all = parsed.has('a');
    bool s = all || parsed.has('s');
    bool n = all || parsed.has('n');
    bool r = all || parsed.has('r');
    bool v = all || parsed.has('v');
    bool m = all || parsed.has('m');

    // Default: print kernel name
    if (!s && !n && !r && !v && !m) s = true;

    bool first = true;
    auto append = [&](const char* str) {
        if (!first) std::fputc(' ', stdout);
        std::fputs(str, stdout);
        first = false;
    };

    if (s) append(info.sysname);
    if (n) append(info.nodename);
    if (r) append(info.release);
    if (v) append(info.version);
    if (m) append(info.machine);

    std::fputc('\n', stdout);
    return 0;
}
