#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <string>
#include <string_view>
#include <algorithm>

#include <cfbox/applet.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>
#include <cfbox/args.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "ps",
    .version = CFBOX_VERSION_STRING,
    .one_line = "report a snapshot of current processes",
    .usage   = "ps [aux] [-e]",
    .options = "  aux    show all processes with user-oriented format\n"
               "  -e     show all processes (simple format)",
    .extra   = "",
};

auto uid_to_name(uid_t uid) -> std::string {
    // Simple numeric representation
    return std::to_string(uid);
}

auto format_time_ticks(std::uint64_t ticks) -> std::string {
    auto total_secs = static_cast<long>(ticks / cfbox::proc::clock_ticks_per_second());
    auto mins = total_secs / 60;
    auto secs = total_secs % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%ld:%02ld", mins, secs);
    return buf;
}

auto format_start_time(std::uint64_t start_ticks) -> std::string {
    auto start_secs = static_cast<time_t>(start_ticks / static_cast<std::uint64_t>(cfbox::proc::clock_ticks_per_second()));

    auto up_result = cfbox::proc::read_uptime();
    if (!up_result) return "?";

    time_t boot_time = static_cast<time_t>(static_cast<double>(std::time(nullptr)) - up_result->first);
    time_t proc_start = boot_time + start_secs;

    auto now = std::time(nullptr);
    auto tm = std::localtime(&proc_start);
    char buf[16];

    if (now - proc_start < 86400) {
        // Today: show HH:MM
        std::strftime(buf, sizeof(buf), "%H:%M", tm);
    } else {
        // Older: show MonDD
        std::strftime(buf, sizeof(buf), "%b%d", tm);
    }
    return buf;
}

auto format_state(char state) -> std::string {
    switch (state) {
    case 'R': return "R";
    case 'S': return "S";
    case 'D': return "D";
    case 'Z': return "Z";
    case 'T': return "T";
    case 't': return "t";
    case 'W': return "W";
    default: return std::string(1, state);
    }
}

} // anonymous namespace

auto ps_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'e', false},
        cfbox::args::OptSpec{'f', false},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    // Detect format: "aux" as a single arg, or flags
    bool aux_format = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "aux" || arg == "-aux" || arg == "auxf") {
            aux_format = true;
        }
    }
    (void)parsed.has('e'); // -e currently unused

    auto result = cfbox::proc::read_all_processes();
    if (!result) {
        std::fprintf(stderr, "cfbox ps: %s\n", result.error().msg.c_str());
        return 1;
    }

    auto total_mem = cfbox::proc::total_memory_kb();
    auto ticks = cfbox::proc::clock_ticks_per_second();

    if (aux_format) {
        std::printf("%-8s %5s %4s %4s %8s %8s %-6s %-4s %-6s %5s %s\n",
                    "USER", "PID", "%CPU", "%MEM", "VSZ", "RSS", "TTY", "STAT",
                    "START", "TIME", "COMMAND");

        for (const auto& p : *result) {
            auto user = uid_to_name(p.uid);

            // CPU%: rough approximation from utime+stime / total time since start
            double cpu_pct = 0.0;
            if (ticks > 0 && p.start_time > 0) {
                auto up_result = cfbox::proc::read_uptime();
                if (up_result) {
                    double uptime = up_result->first;
                    double dticks = static_cast<double>(p.start_time);
                    double secs = uptime - dticks / static_cast<double>(ticks);
                    if (secs > 0) {
                        cpu_pct = static_cast<double>(p.utime + p.stime) / static_cast<double>(ticks) / secs * 100.0;
                    }
                }
            }

            double mem_pct = 0.0;
            if (total_mem > 0) {
                double rss_kb = static_cast<double>(p.rss) * static_cast<double>(cfbox::proc::page_size()) / 1024.0;
                mem_pct = rss_kb / static_cast<double>(total_mem) * 100.0;
            }

            auto vsz = p.vsize / 1024; // to KiB
            auto rss = p.rss * static_cast<std::uint64_t>(cfbox::proc::page_size()) / 1024;
            auto stat = format_state(p.state);
            auto start = format_start_time(p.start_time);
            auto time = format_time_ticks(p.utime + p.stime);

            // Command: use cmdline if available, otherwise [comm]
            std::string cmd;
            if (!p.cmdline.empty()) {
                for (std::size_t i = 0; i < p.cmdline.size(); ++i) {
                    if (i > 0) cmd += ' ';
                    cmd += p.cmdline[i];
                }
            } else {
                cmd = "[" + p.comm + "]";
            }

            std::printf("%-8s %5d %4.1f %4.1f %8llu %8llu %-6s %-4s %-6s %5s %s\n",
                        user.c_str(), p.pid, cpu_pct, mem_pct,
                        static_cast<unsigned long long>(vsz),
                        static_cast<unsigned long long>(rss),
                        p.tty.c_str(), stat.c_str(),
                        start.c_str(), time.c_str(), cmd.c_str());
        }
    } else {
        // Simple format: PID TTY TIME CMD
        std::printf("  PID TTY          TIME CMD\n");
        for (const auto& p : *result) {
            auto time = format_time_ticks(p.utime + p.stime);
            std::string cmd = p.cmdline.empty() ? ("[" + p.comm + "]") : p.cmdline[0];
            std::printf("%5d %-6s %10s %s\n", p.pid, p.tty.c_str(), time.c_str(), cmd.c_str());
        }
    }

    return 0;
}
