#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/proc.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "pmap",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display memory map of a process",
    .usage   = "pmap [-x] PID",
    .options = "  -x     extended format",
    .extra   = "",
};

struct MapEntry {
    std::uint64_t address = 0;
    std::uint64_t end_address = 0;
    std::string perms;
    std::uint64_t offset = 0;
    std::string dev;
    std::uint64_t inode = 0;
    std::string pathname;
};

auto parse_maps(pid_t pid) -> std::vector<MapEntry> {
    auto path = "/proc/" + std::to_string(pid) + "/maps";
    auto opened = cfbox::io::open_file(path, "r");
    if (!opened) return {};

    std::vector<MapEntry> entries;
    char line[1024];
    while (std::fgets(line, sizeof(line), opened->get())) {
        auto len = std::strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        MapEntry e;
        char perms[8] = {};
        char dev[16] = {};
        unsigned long long addr = 0;
        unsigned long long off = 0;
        unsigned long long ino = 0;

        auto n = std::sscanf(line, "%llx-%*x %7s %llx %15s %llu",
                             &addr, perms, &off, dev, &ino);
        if (n < 1) continue;

        e.address = addr;
        e.perms = perms;
        e.offset = off;
        e.dev = dev;
        e.inode = ino;

        // Extract pathname: find the field after inode
        const char* p = line;
        // Skip address field
        while (*p && *p != ' ') ++p;
        // Skip to perms
        while (*p == ' ') ++p;
        // Skip perms
        while (*p && *p != ' ') ++p;
        // Skip to offset
        while (*p == ' ') ++p;
        // Skip offset
        while (*p && *p != ' ') ++p;
        // Skip to dev
        while (*p == ' ') ++p;
        // Skip dev
        while (*p && *p != ' ') ++p;
        // Skip to inode
        while (*p == ' ') ++p;
        // Skip inode
        while (*p && *p != ' ') ++p;
        // Skip spaces to pathname
        while (*p == ' ') ++p;
        if (*p) e.pathname = p;

        entries.push_back(std::move(e));
    }

    // Calculate end_address from next entry
    for (size_t i = 0; i + 1 < entries.size(); ++i) {
        entries[i].end_address = entries[i + 1].address;
    }

    return entries;
}

} // anonymous namespace

auto pmap_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'x', false, "extended"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool extended = parsed.has('x') || parsed.has_long("extended");

    const auto& args = parsed.positional();
    if (args.empty()) {
        CFBOX_ERR("pmap", "no PID specified");
        return 1;
    }

    pid_t pid = static_cast<pid_t>(std::strtol(args[0].data(), nullptr, 10));
    auto entries = parse_maps(pid);
    if (entries.empty()) {
        CFBOX_ERR("pmap", "cannot read maps for PID %d", pid);
        return 1;
    }

    std::printf("%d:   %s\n", pid, std::string(args[0]).c_str());

    std::uint64_t total_kb = 0;
    for (const auto& e : entries) {
        auto kb = (e.end_address - e.address) / 1024;
        total_kb += kb;

        if (extended) {
            std::printf("%016lx %5llu %08lx   %-5s %-6s %6llu %s\n",
                        static_cast<unsigned long>(e.address),
                        static_cast<unsigned long long>(kb),
                        static_cast<unsigned long>(e.offset),
                        e.perms.c_str(),
                        e.dev.c_str(),
                        static_cast<unsigned long long>(e.inode),
                        e.pathname.c_str());
        } else {
            std::printf("%016lx %5luK %s %s\n",
                        static_cast<unsigned long>(e.address),
                        static_cast<unsigned long>(kb),
                        e.perms.c_str(),
                        e.pathname.empty() ? "" : e.pathname.c_str());
        }
    }

    std::printf(" mapped: %lluK\n",
                static_cast<unsigned long long>(total_kb));

    return 0;
}
