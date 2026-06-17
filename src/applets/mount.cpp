#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <sys/mount.h>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "mount",
    .version = CFBOX_VERSION_STRING,
    .one_line = "mount a filesystem",
    .usage = "mount [-a] [-t TYPE] [-o OPTS] [-r] [SOURCE TARGET]",
    .options = "  -a       mount all entries listed in /etc/fstab\n"
               "  -t TYPE  filesystem type (e.g. proc, devpts, tmpfs)\n"
               "  -o OPTS  comma-separated mount options (ro,rw,nosuid,...)\n"
               "  -r       mount read-only",
    .extra = "",
};

struct FstabEntry {
    std::string fs;
    std::string mountpoint;
    std::string type;
    std::string options;
};

struct MountOpts {
    unsigned long flags = 0;
    std::string data; // fs-specific options that do not map to VFS flags
};

// Split a comma-separated option string into VFS mount flags plus the leftover
// fs-specific data string. Mirrors util-linux's flag mapping for the common
// cases BusyBox rootfs scripts rely on (ro/rw/defaults/nosuid/...).
auto parse_mount_options(std::string_view opts) -> MountOpts {
    MountOpts out;
    std::string data;
    std::size_t i = 0;
    while (i <= opts.size()) {
        auto comma = opts.find(',', i);
        auto end = (comma == std::string_view::npos) ? opts.size() : comma;
        auto token = opts.substr(i, end - i);

        if (token == "ro")
            out.flags |= MS_RDONLY;
        else if (token == "rw")
            out.flags &= ~MS_RDONLY;
        else if (token == "nosuid")
            out.flags |= MS_NOSUID;
        else if (token == "nodev")
            out.flags |= MS_NODEV;
        else if (token == "noexec")
            out.flags |= MS_NOEXEC;
        else if (token == "noatime")
            out.flags |= MS_NOATIME;
        else if (token == "relatime")
            out.flags |= MS_RELATIME;
        else if (token == "sync")
            out.flags |= MS_SYNCHRONOUS;
        else if (token == "remount")
            out.flags |= MS_REMOUNT;
        else if (token == "bind")
            out.flags |= MS_BIND;
        else if (token == "defaults" || token.empty()) { /* no-op */
        } else {
            if (!data.empty())
                data += ',';
            data += std::string(token);
        }

        if (comma == std::string_view::npos)
            break;
        i = comma + 1;
    }
    out.data = std::move(data);
    return out;
}

// Parse /etc/fstab into mount entries. Skips blank lines and '#' comments;
// fields beyond options (dump/pass) are ignored.
auto parse_fstab(const std::string& path) -> std::vector<FstabEntry> {
    std::vector<FstabEntry> entries;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f)
        return entries;

    char buf[512];
    while (std::fgets(buf, sizeof(buf), f)) {
        std::string_view line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.remove_suffix(1);

        auto first = line.find_first_not_of(" \t");
        if (first == std::string_view::npos)
            continue; // blank
        if (line[first] == '#')
            continue; // comment

        FstabEntry e;
        std::size_t pos = first;
        int field = 0;
        while (field < 4 && pos < line.size()) {
            while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
                ++pos;
            if (pos >= line.size())
                break;
            auto start = pos;
            while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
                ++pos;
            auto tok = line.substr(start, pos - start);
            switch (field) {
                case 0:
                    e.fs = std::string(tok);
                    break;
                case 1:
                    e.mountpoint = std::string(tok);
                    break;
                case 2:
                    e.type = std::string(tok);
                    break;
                case 3:
                    e.options = std::string(tok);
                    break;
            }
            ++field;
        }
        if (!e.fs.empty() && !e.mountpoint.empty()) {
            entries.push_back(std::move(e));
        }
    }
    std::fclose(f);
    return entries;
}

auto mount_one(std::string_view source, std::string_view target, std::string_view type,
               std::string_view opts) -> int {
    auto mo = parse_mount_options(opts);
    std::string src(source);
    std::string tgt(target);
    std::string ty(type);
    const char* data = mo.data.empty() ? nullptr : mo.data.c_str();
    if (::mount(src.c_str(), tgt.c_str(), ty.empty() ? nullptr : ty.c_str(), mo.flags, data) != 0) {
        CFBOX_ERR("mount", "%s: %s", tgt.c_str(), std::strerror(errno));
        return 1;
    }
    return 0;
}

} // namespace

auto mount_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv,
                                     {
                                         cfbox::args::OptSpec{'a', false, "all"},
                                         cfbox::args::OptSpec{'t', true, "types"},
                                         cfbox::args::OptSpec{'o', true, "options"},
                                         cfbox::args::OptSpec{'r', false, "read-only"},
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
    auto type = parsed.get('t');
    auto opts = parsed.get('o');
    const auto& pos = parsed.positional();

    std::string opts_str = opts ? std::string(*opts) : std::string{};
    if (ro)
        opts_str = opts_str.empty() ? std::string("ro") : "ro," + opts_str;

    if (all) {
        auto entries = parse_fstab("/etc/fstab");
        if (entries.empty()) {
            CFBOX_ERR("mount", "no usable entries in /etc/fstab");
            return 1;
        }
        int rc = 0;
        for (const auto& e : entries) {
            if (mount_one(e.fs, e.mountpoint, e.type, e.options) != 0)
                rc = 1;
        }
        return rc;
    }

    // single mount: SOURCE TARGET
    if (pos.size() < 2) {
        cfbox::help::print_help(HELP);
        return 2;
    }
    std::string type_str = type ? std::string(*type) : std::string{};
    return mount_one(pos[0], pos[1], type_str, opts_str);
}
