#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "mkfifo",
    .version = CFBOX_VERSION_STRING,
    .one_line = "make FIFOs (named pipes)",
    .usage   = "mkfifo [NAME]...",
    .options = "  -m MODE  set permission mode (octal, default 0644)",
    .extra   = "",
};
} // namespace

auto mkfifo_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'m', true, "mode"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto mode_str = parsed.get_any('m', "mode");
    mode_t mode = 0644;
    if (mode_str) {
        mode = static_cast<mode_t>(std::stoul(std::string{*mode_str}, nullptr, 8));
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox mkfifo: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        std::string path{p};
        if (mkfifo(path.c_str(), mode) != 0) {
            std::fprintf(stderr, "cfbox mkfifo: cannot create fifo '%s': %s\n",
                         path.c_str(), std::strerror(errno));
            rc = 1;
        }
    }
    return rc;
}
