#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "mountpoint",
    .version = CFBOX_VERSION_STRING,
    .one_line = "check if a path is a mountpoint",
    .usage   = "mountpoint [-q] [-d] PATH",
    .options = "  -q     quiet mode\n  -d     print major:minor device numbers",
    .extra   = "",
};

} // namespace

auto mountpoint_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'q', false, "quiet"},
        cfbox::args::OptSpec{'d', false, "device"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool quiet = parsed.has('q');
    bool dev = parsed.has('d');
    const auto& pos = parsed.positional();
    if (pos.empty()) {
        CFBOX_ERR("mountpoint", "missing argument");
        return 2;
    }

    std::string target(pos[0]);
    struct stat st;
    if (stat(target.c_str(), &st) != 0) {
        if (!quiet) CFBOX_ERR("mountpoint", "%s: %s", target.c_str(), std::strerror(errno));
        return 1;
    }

    // Check if target is a mountpoint by comparing st_dev with parent
    if (dev) {
        std::printf("%u:%u\n", major(st.st_dev), minor(st.st_dev));
        return 0;
    }

    // Compare device numbers with parent directory
    std::string parent = target;
    if (!parent.empty() && parent.back() == '/') parent.pop_back();
    auto slash = parent.rfind('/');
    if (slash == 0) parent = "/";
    else if (slash != std::string::npos) parent.resize(slash);

    struct stat parent_st;
    bool is_mountpoint = false;
    if (stat(parent.c_str(), &parent_st) == 0) {
        is_mountpoint = (st.st_dev != parent_st.st_dev);
    }

    // Also verify against /proc/mounts for symlink resolution
    auto mounts_result = cfbox::proc::read_mounts();
    if (mounts_result) {
        for (const auto& m : *mounts_result) {
            if (m.mountpoint == target) {
                is_mountpoint = true;
                break;
            }
        }
    }

    if (is_mountpoint) {
        if (!quiet) std::printf("%s is a mountpoint\n", target.c_str());
        return 0;
    } else {
        if (!quiet) std::printf("%s is not a mountpoint\n", target.c_str());
        return 1;
    }
}
