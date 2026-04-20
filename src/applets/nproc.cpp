#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "nproc",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print number of available processors",
    .usage   = "nproc [--all] [--ignore=N]",
    .options = "  --all       print installed processors\n"
               "  --ignore=N  subtract N from the count",
    .extra   = "",
};
} // namespace

auto nproc_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'\0', false, "all"},
        cfbox::args::OptSpec{'\0', true, "ignore"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    long count;
    if (parsed.has_long("all")) {
        count = sysconf(_SC_NPROCESSORS_CONF);
    } else {
        count = sysconf(_SC_NPROCESSORS_ONLN);
    }

    if (count <= 0) {
        count = static_cast<long>(std::thread::hardware_concurrency());
    }
    if (count <= 0) count = 1;

    auto ignore_val = parsed.get_long("ignore");
    if (ignore_val) {
        long ignore = std::strtol(std::string{*ignore_val}.c_str(), nullptr, 10);
        count -= ignore;
        if (count < 1) count = 1;
    }

    std::printf("%ld\n", count);
    return 0;
}
