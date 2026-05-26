#include <cstdio>
#include <cstring>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "touch",
    .version = CFBOX_VERSION_STRING,
    .one_line = "change file timestamps",
    .usage   = "touch [-c] FILE...",
    .options = "  -c     do not create any files",
    .extra   = "",
};
} // namespace

auto touch_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'c', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool no_create = parsed.has('c');
    const auto& pos = parsed.positional();

    if (pos.empty()) {
        CFBOX_ERR("touch", "missing operand");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        std::string path{p};
        if (::access(path.c_str(), F_OK) != 0) {
            if (no_create) continue;
            int fd = ::open(path.c_str(), O_CREAT | O_WRONLY, 0666);
            if (fd < 0) {
                CFBOX_ERR("touch", "cannot touch '%s': %s", path.c_str(), std::strerror(errno));
                rc = 1;
                continue;
            }
            ::close(fd);
        }
        // Update timestamps
        if (::utime(path.c_str(), nullptr) != 0) {
            CFBOX_ERR("touch", "cannot touch '%s': %s", path.c_str(), std::strerror(errno));
            rc = 1;
        }
    }
    return rc;
}
