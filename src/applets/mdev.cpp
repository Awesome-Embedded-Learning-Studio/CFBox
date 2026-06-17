#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "mdev",
    .version = CFBOX_VERSION_STRING,
    .one_line = "populate /dev from sysfs (coldplug scan)",
    .usage = "mdev -s",
    .options = "  -s    scan /sys and create device nodes under /dev",
    .extra = "",
};

struct SysDevice {
    std::string name;
    int major = -1;
    int minor = -1;
    bool is_block = false;
};

// Read "<major>:<minor>" from <dir>/dev; absent or unparseable => nullopt.
auto read_dev(const std::string& dir) -> std::optional<std::pair<int, int>> {
    FILE* f = std::fopen((dir + "/dev").c_str(), "r");
    if (!f)
        return std::nullopt;
    int maj = -1;
    int min = -1;
    int n = std::fscanf(f, "%d:%d", &maj, &min);
    std::fclose(f);
    if (n != 2 || maj < 0)
        return std::nullopt;
    return std::make_pair(maj, min);
}

auto collect_dir(const std::string& dir, bool is_block, std::vector<SysDevice>& out) -> void {
    DIR* d = opendir(dir.c_str());
    if (!d)
        return;
    while (auto* de = readdir(d)) {
        if (de->d_name[0] == '.')
            continue;
        std::string sub = dir + "/" + de->d_name;
        if (auto dv = read_dev(sub)) {
            out.push_back({de->d_name, dv->first, dv->second, is_block});
        }
    }
    closedir(d);
}

// Walk /sys/class/* (char devices) and /sys/block (block devices plus their
// partitions), collecting every entry that exposes a major:minor dev file.
auto walk_sysfs(const std::string& sysroot) -> std::vector<SysDevice> {
    std::vector<SysDevice> out;

    std::string classdir = sysroot + "/class";
    if (DIR* d = opendir(classdir.c_str())) {
        while (auto* de = readdir(d)) {
            if (de->d_name[0] == '.')
                continue;
            collect_dir(classdir + "/" + de->d_name, false, out);
        }
        closedir(d);
    }

    std::string blockdir = sysroot + "/block";
    if (DIR* d = opendir(blockdir.c_str())) {
        while (auto* de = readdir(d)) {
            if (de->d_name[0] == '.')
                continue;
            std::string blk = blockdir + "/" + de->d_name;
            if (auto dv = read_dev(blk)) {
                out.push_back({de->d_name, dv->first, dv->second, true});
            }
            collect_dir(blk, true, out); // partitions: /sys/block/<dev>/<part>
        }
        closedir(d);
    }

    return out;
}

auto make_node(const SysDevice& dev) -> int {
    std::string path = "/dev/" + dev.name;
    mode_t type = dev.is_block ? S_IFBLK : S_IFCHR;
    unlink(path.c_str()); // replace stale node so attributes stay current
    if (mknod(path.c_str(), type | 0660, makedev(dev.major, dev.minor)) != 0) {
        CFBOX_ERR("mdev", "%s: %s", path.c_str(), std::strerror(errno));
        return 1;
    }
    return 0;
}

} // namespace

auto mdev_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv,
                                     {
                                         cfbox::args::OptSpec{'s', false, "scan"},
                                     });

    if (parsed.has_long("help")) {
        cfbox::help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        cfbox::help::print_version(HELP);
        return 0;
    }

    if (!parsed.has('s')) {
        // Hotplug mode (kernel-invoked, uevent via environment) is not
        // implemented; imx-forge only ever runs the coldplug scan `mdev -s`.
        return 0;
    }

    int rc = 0;
    for (const auto& dev : walk_sysfs("/sys")) {
        if (make_node(dev) != 0)
            rc = 1;
    }
    return rc;
}
