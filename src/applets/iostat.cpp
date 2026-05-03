#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "iostat",
    .version = CFBOX_VERSION_STRING,
    .one_line = "report CPU and I/O statistics",
    .usage   = "iostat [-c COUNT] [-d DELAY]",
    .options = "  -c N   number of reports (default 1)\n"
               "  -d N   delay in seconds between reports (default 1)",
    .extra   = "",
};

auto print_iostat(const std::vector<cfbox::proc::DiskStat>& stats) -> void {
    std::printf("%-10s %10s %10s %10s %10s %10s\n",
                "Device", "r/s", "w/s", "rkB/s", "wkB/s", "await");
    for (const auto& ds : stats) {
        if (ds.device.find("ram") != std::string::npos ||
            ds.device.find("loop") != std::string::npos) continue;

        std::printf("%-10s %10llu %10llu %10llu %10llu %10llu\n",
                    ds.device.c_str(),
                    static_cast<unsigned long long>(ds.reads),
                    static_cast<unsigned long long>(ds.writes),
                    static_cast<unsigned long long>(ds.sectors_read / 2),
                    static_cast<unsigned long long>(ds.sectors_written / 2),
                    static_cast<unsigned long long>(ds.ms_ios));
    }
}

auto print_delta(const std::vector<cfbox::proc::DiskStat>& prev,
                 const std::vector<cfbox::proc::DiskStat>& curr,
                 double interval) -> void {
    std::printf("%-10s %10s %10s %10s %10s\n",
                "Device", "r/s", "w/s", "rkB/s", "wkB/s");
    for (const auto& c : curr) {
        if (c.device.find("ram") != std::string::npos ||
            c.device.find("loop") != std::string::npos) continue;

        double dr = static_cast<double>(c.reads);
        double dw = static_cast<double>(c.writes);
        double drkb = static_cast<double>(c.sectors_read / 2);
        double dwkb = static_cast<double>(c.sectors_written / 2);
        for (const auto& p : prev) {
            if (p.device == c.device) {
                dr = static_cast<double>(c.reads - p.reads) / interval;
                dw = static_cast<double>(c.writes - p.writes) / interval;
                drkb = static_cast<double>((c.sectors_read - p.sectors_read) / 2) / interval;
                dwkb = static_cast<double>((c.sectors_written - p.sectors_written) / 2) / interval;
                break;
            }
        }
        std::printf("%-10s %10.0f %10.0f %10.0f %10.0f\n",
                    c.device.c_str(), dr, dw, drkb, dwkb);
    }
}

} // anonymous namespace

auto iostat_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'c', true, "count"},
        cfbox::args::OptSpec{'d', true, "delay"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int count = 1;
    double delay = 1.0;
    if (auto v = parsed.get('c')) count = std::stoi(std::string(*v));
    if (auto v = parsed.get('d')) delay = std::stod(std::string(*v));

    auto first = cfbox::proc::read_diskstats();
    if (!first) {
        std::fprintf(stderr, "cfbox iostat: %s\n", first.error().msg.c_str());
        return 1;
    }

    auto cpu = cfbox::proc::read_cpu_stats();
    if (cpu) {
        double total = static_cast<double>(cpu->total());
        if (total > 0.0) {
            auto pct = [&](std::uint64_t v) -> double {
                return 100.0 * static_cast<double>(v) / total;
            };
            double idle_pct = pct(cpu->idle_time());
            std::printf("avg-cpu:  %%user   %%nice    %%system  %%iowait  %%steal   %%idle\n");
            std::printf("          %6.1f %6.1f %9.1f %8.1f %7.1f %7.1f\n",
                        pct(cpu->user), pct(cpu->nice), pct(cpu->system),
                        pct(cpu->iowait), pct(cpu->steal), idle_pct);
        }
        std::printf("\n");
    }

    if (count <= 1) {
        print_iostat(*first);
        return 0;
    }

    auto prev = *first;
    for (int i = 1; i < count; ++i) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(delay * 1000)));

        auto curr = cfbox::proc::read_diskstats();
        if (!curr) break;

        std::printf("\n");
        print_delta(prev, *curr, delay);
        prev = *curr;
    }

    return 0;
}
