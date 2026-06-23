#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/swap.h>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "swapoff",
    .version = CFBOX_VERSION_STRING,
    .one_line = "disable swap",
    .usage = "swapoff [-a] [DEVICE]",
    .options = "  -a    disable all swap devices listed in /proc/swaps",
    .extra = "",
};

// First whitespace-delimited token of a /proc/swaps line (the device path).
auto first_token(const char* line) -> std::string {
    std::string dev;
    for (const char* p = line; *p && *p != ' ' && *p != '\t' && *p != '\n'; ++p)
        dev += *p;
    return dev;
}

} // namespace

auto swapoff_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv,
                                     {
                                         cfbox::args::OptSpec{'a', false, "all"},
                                     });

    if (parsed.has_long("help")) {
        cfbox::help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        cfbox::help::print_version(HELP);
        return 0;
    }

    if (parsed.has('a')) {
        FILE* f = std::fopen("/proc/swaps", "r");
        if (!f)
            return 0; // no swap configured — nothing to do
        char line[512];
        if (!std::fgets(line, sizeof(line), f)) { // skip header; empty swaps file is fine
            std::fclose(f);
            return 0;
        }
        int rc = 0;
        while (std::fgets(line, sizeof(line), f)) {
            std::string dev = first_token(line);
            if (dev.empty())
                continue;
            if (::swapoff(dev.c_str()) != 0) {
                CFBOX_ERR("swapoff", "%s: %s", dev.c_str(), std::strerror(errno));
                rc = 1;
            }
        }
        std::fclose(f);
        return rc;
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        cfbox::help::print_help(HELP);
        return 2;
    }
    std::string dev(pos[0]);
    if (::swapoff(dev.c_str()) != 0) {
        CFBOX_ERR("swapoff", "%s: %s", dev.c_str(), std::strerror(errno));
        return 1;
    }
    return 0;
}
