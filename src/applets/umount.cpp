#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

#include <cstring>
#include <string>
#include <sys/mount.h>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "umount",
    .version = CFBOX_VERSION_STRING,
    .one_line = "unmount filesystems",
    .usage = "umount [-a] [-r] [-f] [TARGET]",
    .options = "  -a    unmount all (except / /proc /sys /dev)\n"
               "  -r    remount read-only if unmount fails\n"
               "  -f    force unmount (MNT_FORCE)",
    .extra = "",
};

auto is_protected(const std::string& mp) -> bool {
    return mp == "/" || mp == "/proc" || mp == "/sys" || mp == "/dev";
}

// Try to drop a mount; on failure, optionally fall back to a read-only remount.
auto drop_one(const std::string& target, int flags, bool ro) -> int {
    if (umount2(target.c_str(), flags) == 0)
        return 0;
    if (ro && mount(nullptr, target.c_str(), nullptr, MS_REMOUNT | MS_RDONLY, nullptr) == 0)
        return 0;
    CFBOX_ERR("umount", "%s: %s", target.c_str(), std::strerror(errno));
    return 1;
}

} // namespace

auto umount_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv,
                                     {
                                         cfbox::args::OptSpec{'a', false, "all"},
                                         cfbox::args::OptSpec{'r', false, "read-only"},
                                         cfbox::args::OptSpec{'f', false, "force"},
                                     });

    if (parsed.has_long("help")) {
        cfbox::help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        cfbox::help::print_version(HELP);
        return 0;
    }

    bool all = parsed.has('a');
    bool ro = parsed.has('r');
    int flags = parsed.has('f') ? MNT_FORCE : 0;
    const auto& pos = parsed.positional();

    if (all) {
        auto mounts = cfbox::proc::read_mounts();
        if (!mounts) {
            CFBOX_ERR("umount", "cannot read /proc/mounts");
            return 1;
        }
        const auto& vec = *mounts;
        int rc = 0;
        for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
            if (is_protected(it->mountpoint))
                continue; // never drop the boot fs
            if (drop_one(it->mountpoint, flags, ro) != 0)
                rc = 1;
        }
        return rc;
    }

    if (pos.empty()) {
        cfbox::help::print_help(HELP);
        return 2;
    }
    return drop_one(std::string(pos[0]), flags, ro);
}
