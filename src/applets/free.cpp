#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "free",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display amount of free and used memory",
    .usage   = "free [-b|-k|-m|-g|-h]",
    .options = "  -b     show bytes\n"
               "  -k     show kilobytes (default)\n"
               "  -m     show megabytes\n"
               "  -g     show gigabytes\n"
               "  -h     human-readable",
    .extra   = "",
};

auto human_size(std::uint64_t kb) -> std::string {
    if (kb >= 1024ULL * 1024 * 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fGi", static_cast<double>(kb) / (1024.0 * 1024.0 * 1024.0));
        return buf;
    }
    if (kb >= 1024ULL * 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fMi", static_cast<double>(kb) / (1024.0 * 1024.0));
        return buf;
    }
    if (kb >= 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1fMi", static_cast<double>(kb) / 1024.0);
        return buf;
    }
    return std::to_string(kb) + "Ki";
}

auto format_val(std::uint64_t kb, bool human, char unit) -> std::string {
    if (human) return human_size(kb);
    switch (unit) {
    case 'b': return std::to_string(kb * 1024);
    case 'm': {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(kb) / 1024.0);
        return buf;
    }
    case 'g': {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(kb) / (1024.0 * 1024.0));
        return buf;
    }
    default: return std::to_string(kb);
    }
}

} // anonymous namespace

auto free_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'b', false},
        cfbox::args::OptSpec{'k', false},
        cfbox::args::OptSpec{'m', false},
        cfbox::args::OptSpec{'g', false},
        cfbox::args::OptSpec{'h', false, "human"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto result = cfbox::proc::read_meminfo();
    if (!result) {
        std::fprintf(stderr, "cfbox free: %s\n", result.error().msg.c_str());
        return 1;
    }
    const auto& mi = *result;

    bool human = parsed.has('h') || parsed.has_long("human");
    char unit = 'k';
    if (parsed.has('b')) unit = 'b';
    else if (parsed.has('m')) unit = 'm';
    else if (parsed.has('g')) unit = 'g';

    auto used = mi.total - mi.free - mi.buffers - mi.cached;
    auto buff_cache = mi.buffers + mi.cached;

    std::printf("%-16s %12s %12s %12s %12s %12s\n",
                "total", "used", "free", "shared", "buff/cache", "available");

    auto fmt = [&](std::uint64_t v) { return format_val(v, human, unit); };

    std::printf("Mem:    %12s %12s %12s %12s %12s %12s\n",
                fmt(mi.total).c_str(), fmt(used).c_str(), fmt(mi.free).c_str(),
                fmt(mi.shmem).c_str(), fmt(buff_cache).c_str(), fmt(mi.available).c_str());

    if (mi.swap_total > 0) {
        auto swap_used = mi.swap_total - mi.swap_free;
        std::printf("Swap:   %12s %12s %12s\n",
                    fmt(mi.swap_total).c_str(), fmt(swap_used).c_str(), fmt(mi.swap_free).c_str());
    }

    return 0;
}
