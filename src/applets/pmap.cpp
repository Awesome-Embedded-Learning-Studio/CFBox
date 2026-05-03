#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

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
    std::ifstream f(path);
    if (!f) return {};

    std::vector<MapEntry> entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        MapEntry e;

        auto dash = line.find('-');
        if (dash == std::string::npos) continue;
        e.address = std::strtoull(line.substr(0, dash).c_str(), nullptr, 16);

        // Find the space after perms to get end_address
        auto rest = line.substr(dash + 1);
        std::istringstream iss(rest);
        std::string perms_str;
        iss >> perms_str;
        e.end_address = e.address + 1; // Will be properly calculated from next line
        e.perms = perms_str;
        iss >> std::hex >> e.offset;

        std::string dev_str;
        iss >> dev_str;
        e.dev = dev_str;

        iss >> std::dec >> e.inode;

        // Remaining is pathname (may be empty)
        std::string pn;
        while (iss.peek() == ' ') iss.get();
        if (std::getline(iss, pn)) {
            // Trim leading space
            auto start = pn.find_first_not_of(' ');
            if (start != std::string::npos) e.pathname = pn.substr(start);
        }

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
        std::fprintf(stderr, "cfbox pmap: no PID specified\n");
        return 1;
    }

    pid_t pid = static_cast<pid_t>(std::stoi(std::string(args[0])));
    auto entries = parse_maps(pid);
    if (entries.empty()) {
        std::fprintf(stderr, "cfbox pmap: cannot read maps for PID %d\n", pid);
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
