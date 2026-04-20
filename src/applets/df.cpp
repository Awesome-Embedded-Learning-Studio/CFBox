#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/statvfs.h>
#include <mntent.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "df",
    .version = CFBOX_VERSION_STRING,
    .one_line = "report file system disk space usage",
    .usage   = "df [-h] [FILE]...",
    .options = "  -h     print sizes in human readable format",
    .extra   = "",
};

static auto human_size(unsigned long long bytes) -> std::string {
    constexpr const char* units[] = {"", "K", "M", "G", "T"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024 && unit < 4) { size /= 1024; ++unit; }
    char buf[32];
    if (unit == 0) std::snprintf(buf, sizeof(buf), "%llu", bytes);
    else std::snprintf(buf, sizeof(buf), "%.1f%s", size, units[unit]);
    return buf;
}
} // namespace

auto df_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'h', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool human = parsed.has('h');
    const auto& pos = parsed.positional();

    struct Entry {
        std::string fs;
        std::string mount;
        unsigned long long total;
        unsigned long long used;
        unsigned long long avail;
        unsigned long long blocks;
    };

    std::vector<Entry> entries;

    if (!pos.empty()) {
        for (auto p : pos) {
            struct statvfs vfs;
            if (statvfs(std::string{p}.c_str(), &vfs) != 0) {
                std::fprintf(stderr, "cfbox df: cannot stat '%.*s': %s\n",
                             static_cast<int>(p.size()), p.data(), std::strerror(errno));
                continue;
            }
            auto bsize = static_cast<unsigned long long>(vfs.f_bsize);
            entries.push_back({
                std::string{p}, std::string{p},
                vfs.f_blocks * bsize,
                (vfs.f_blocks - vfs.f_bfree) * bsize,
                vfs.f_bavail * bsize,
                vfs.f_blocks
            });
        }
    } else {
        auto* mtab = setmntent("/proc/mounts", "r");
        if (!mtab) mtab = setmntent("/etc/mtab", "r");
        if (!mtab) {
            std::fprintf(stderr, "cfbox df: cannot read mount table\n");
            return 1;
        }
        struct mntent* mnt;
        while ((mnt = getmntent(mtab)) != nullptr) {
            struct statvfs vfs;
            if (statvfs(mnt->mnt_dir, &vfs) != 0) continue;
            if (vfs.f_blocks == 0) continue;
            auto bsize = static_cast<unsigned long long>(vfs.f_bsize);
            entries.push_back({
                mnt->mnt_fsname, mnt->mnt_dir,
                vfs.f_blocks * bsize,
                (vfs.f_blocks - vfs.f_bfree) * bsize,
                vfs.f_bavail * bsize,
                vfs.f_blocks
            });
        }
        endmntent(mtab);
    }

    std::printf("%-20s %10s %10s %10s %8s %s\n",
                "Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");
    for (const auto& e : entries) {
        auto pct = e.total > 0 ? static_cast<double>(static_cast<long long>(e.used) * 100) /
                                  static_cast<double>(static_cast<long long>(e.total)) : 0.0;
        if (human) {
            std::printf("%-20s %10s %10s %10s %7.0f%% %s\n",
                        e.fs.c_str(),
                        human_size(e.total).c_str(),
                        human_size(e.used).c_str(),
                        human_size(e.avail).c_str(),
                        pct,
                        e.mount.c_str());
        } else {
            auto kblocks = e.blocks;
            auto kused = e.used / 1024;
            auto kavail = e.avail / 1024;
            std::printf("%-20s %10llu %10llu %10llu %7.0f%% %s\n",
                        e.fs.c_str(),
                        kblocks, kused, kavail,
                        pct,
                        e.mount.c_str());
        }
    }
    return 0;
}
