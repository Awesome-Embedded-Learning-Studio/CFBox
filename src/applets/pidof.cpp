#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "pidof",
    .version = CFBOX_VERSION_STRING,
    .one_line = "find the process ID of a running program",
    .usage   = "pidof [-s] NAME...",
    .options = "  -s     single shot — return one PID only",
    .extra   = "",
};

} // anonymous namespace

auto pidof_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false, "single"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool single = parsed.has('s') || parsed.has_long("single");
    const auto& names = parsed.positional();
    if (names.empty()) {
        std::fprintf(stderr, "cfbox pidof: no program name specified\n");
        return 1;
    }

    auto result = cfbox::proc::read_all_processes();
    if (!result) {
        std::fprintf(stderr, "cfbox pidof: %s\n", result.error().msg.c_str());
        return 1;
    }

    bool found = false;
    bool first = true;
    for (const auto& proc : *result) {
        for (const auto& name : names) {
            if (proc.comm == name) {
                if (!first) std::printf(" ");
                std::printf("%d", proc.pid);
                first = false;
                found = true;
                if (single) goto done;
                break;
            }
        }
    }
done:
    if (found) std::printf("\n");
    return found ? 0 : 1;
}
