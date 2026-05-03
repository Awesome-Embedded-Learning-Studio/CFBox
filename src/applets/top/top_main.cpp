#include <cstdio>
#include <string>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/terminal.hpp>
#include <cfbox/tui.hpp>
#include <cfbox/proc.hpp>

#include "top.hpp"

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "top",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display Linux processes",
    .usage   = "top [-d DELAY]",
    .options = "  -d N   delay between updates in seconds (default 3)\n"
               "  -b     batch mode (non-interactive)",
    .extra   = "Keys: q=quit  M=sort MEM  P=sort CPU  T=sort TIME  N=sort PID",
};

class TopApp : public cfbox::tui::TuiApp {
    top::SortField sort_ = top::SortField::Cpu;
    int delay_sec_ = 3;
    bool batch_ = false;
    int iteration_ = 0;
    cfbox::proc::CpuStats prev_cpu_{};
    std::vector<top::ProcEntry> entries_;
public:
    explicit TopApp(int delay, bool batch)
        : TuiApp(delay * 1000), delay_sec_(delay), batch_(batch) {
        prev_cpu_ = cfbox::proc::read_cpu_stats().value_or(cfbox::proc::CpuStats{});
    }

    auto on_key(cfbox::tui::Key k) -> bool override {
        if (k.type == cfbox::tui::KeyType::Escape || k.type == cfbox::tui::KeyType::Ctrl_C)
            return false;
        if (k.is_char()) {
            switch (static_cast<char>(k.ch)) {
                case 'q': return false;
                case 'M': sort_ = top::SortField::Mem; break;
                case 'P': sort_ = top::SortField::Cpu; break;
                case 'T': sort_ = top::SortField::Time; break;
                case 'N': sort_ = top::SortField::Pid; break;
            }
        }
        return true;
    }

    auto on_tick() -> void override {
        auto mem = cfbox::proc::read_meminfo();
        auto total_kb = mem ? mem->total : 1;
        entries_ = top::build_entries(total_kb);

        auto cpu = cfbox::proc::read_cpu_stats();
        if (cpu) {
            double d_total = static_cast<double>(cpu->total() - prev_cpu_.total());
            if (d_total > 0) {
                double ticks = static_cast<double>(cfbox::proc::clock_ticks_per_second());
                for (auto& e : entries_) {
                    e.cpu_pct = static_cast<double>(e.total_time) * ticks / d_total * 100.0;
                }
            }
            prev_cpu_ = *cpu;
        }

        top::sort_entries(entries_, sort_);
        ++iteration_;
    }

    auto on_resize(int /*rows*/, int /*cols*/) -> void override {
        // Nothing extra needed — render() uses screen buffer
    }

    auto sort_field_char() const -> char {
        switch (sort_) {
            case top::SortField::Cpu: return 'P';
            case top::SortField::Mem: return 'M';
            case top::SortField::Pid: return 'N';
            case top::SortField::Time: return 'T';
        }
        return 'P';
    }

    auto run_batch(int count) -> int {
        for (int i = 0; i < count || count <= 0; ++i) {
            on_tick();
            render_to_stdout();
        }
        return 0;
    }

    auto render_to_stdout() -> void {
        auto mem = cfbox::proc::read_meminfo();
        auto up = cfbox::proc::read_uptime();
        auto la = cfbox::proc::read_loadavg();
        if (up && la) {
            auto secs = static_cast<long>(up->first);
            auto hrs = secs / 3600;
            auto mins = (secs % 3600) / 60;
            std::printf("top - up %ld:%02ld, load average: %.2f, %.2f, %.2f\n",
                        hrs, mins, la->avg1, la->avg5, la->avg15);
        }

        std::printf("Tasks: %zu total\n", entries_.size());

        auto cpu = cfbox::proc::read_cpu_stats();
        if (cpu) {
            double t = static_cast<double>(cpu->total());
            if (t > 0) {
                std::printf("%%Cpu: %.1f us, %.1f sy, %.1f id\n",
                            100.0 * static_cast<double>(cpu->user) / t,
                            100.0 * static_cast<double>(cpu->system) / t,
                            100.0 * static_cast<double>(cpu->idle) / t);
            }
        }

        if (mem) {
            std::printf("MiB Mem: %llu total, %llu free, %llu used, %llu cache\n",
                        static_cast<unsigned long long>(mem->total / 1024),
                        static_cast<unsigned long long>(mem->free / 1024),
                        static_cast<unsigned long long>((mem->total - mem->available) / 1024),
                        static_cast<unsigned long long>(mem->cached / 1024));
        }

        std::printf("  PID USER      PR  NI   VIRT    RES  %%CPU  %%MEM  S COMMAND\n");

        for (const auto& e : entries_) {
            std::printf("%5d %-8s %3d %3d %6lluM %5lluM %5.1f %5.1f  %c %s\n",
                        e.pid,
                        e.user.c_str(),
                        e.priority,
                        e.nice_val,
                        static_cast<unsigned long long>(e.vsize / (1024 * 1024)),
                        static_cast<unsigned long long>(e.rss_kb / 1024),
                        e.cpu_pct,
                        e.mem_pct,
                        e.state,
                        e.command.c_str());
        }
    }

    auto render_to_buffer(cfbox::tui::ScreenBuffer& scr) -> void {
        scr.clear();
        int row = 0;

        // Header: uptime and load
        auto up = cfbox::proc::read_uptime();
        auto la = cfbox::proc::read_loadavg();
        if (up && la) {
            auto secs = static_cast<long>(up->first);
            auto mins = secs / 60;
            auto hrs = mins / 60;
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "top - up %ld:%02ld, load average: %.2f, %.2f, %.2f",
                hrs, mins % 60, la->avg1, la->avg5, la->avg15);
            scr.set_string(row, 0, buf);
        }
        ++row;

        // Tasks summary
        char task_buf[128];
        std::snprintf(task_buf, sizeof(task_buf),
            "Tasks: %zu total", entries_.size());
        scr.set_string(row, 0, task_buf);
        ++row;

        // CPU summary
        auto cpu = cfbox::proc::read_cpu_stats();
        if (cpu) {
            char cpu_buf[128];
            auto t = static_cast<double>(cpu->total());
            if (t > 0) {
                std::snprintf(cpu_buf, sizeof(cpu_buf),
                    "%%Cpu: %.1f us, %.1f sy, %.1f id",
                    100.0 * static_cast<double>(cpu->user) / t,
                    100.0 * static_cast<double>(cpu->system) / t,
                    100.0 * static_cast<double>(cpu->idle) / t);
                scr.set_string(row, 0, cpu_buf);
            }
        }
        ++row;

        // Memory summary
        auto mem = cfbox::proc::read_meminfo();
        if (mem) {
            char mem_buf[128];
            std::snprintf(mem_buf, sizeof(mem_buf),
                "MiB Mem: %llu total, %llu free, %llu used, %llu cache",
                static_cast<unsigned long long>(mem->total / 1024),
                static_cast<unsigned long long>(mem->free / 1024),
                static_cast<unsigned long long>((mem->total - mem->available) / 1024),
                static_cast<unsigned long long>(mem->cached / 1024));
            scr.set_string(row, 0, mem_buf);
        }
        ++row;

        // Column headers
        const char* header = "  PID USER      PR  NI   VIRT    RES  %%CPU  %%MEM  S COMMAND";
        scr.set_string(row, 0, header, false, true);
        ++row;

        // Process rows
        auto rows = scr.rows();
        for (size_t i = 0; i < entries_.size() && row < rows - 1; ++i) {
            const auto& e = entries_[i];
            char line[256];
            std::snprintf(line, sizeof(line),
                "%5d %-8s %3d %3d %6lluM %5lluM %5.1f %5.1f  %c %s",
                e.pid,
                e.user.c_str(),
                e.priority,
                e.nice_val,
                static_cast<unsigned long long>(e.vsize / (1024 * 1024)),
                static_cast<unsigned long long>(e.rss_kb / 1024),
                e.cpu_pct,
                e.mem_pct,
                e.state,
                e.command.c_str());
            scr.set_string(row, 0, line);
            ++row;
        }

        // Footer
        if (row < rows) {
            char foot[64];
            std::snprintf(foot, sizeof(foot), "Sort: %c  q=quit  M/P/T/N=sort", sort_field_char());
            scr.set_string(rows - 1, 0, foot, false, true);
        }
    }
};

} // anonymous namespace

auto top_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'d', true, "delay"},
        cfbox::args::OptSpec{'b', false, "batch"},
        cfbox::args::OptSpec{'n', true, "iterations"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int delay = 3;
    bool batch = parsed.has('b') || parsed.has_long("batch");
    int iterations = 0;
    if (auto v = parsed.get('d')) delay = std::stoi(std::string(*v));
    if (auto v = parsed.get('n')) iterations = std::stoi(std::string(*v));
    if (delay < 1) delay = 1;

    if (batch) {
        TopApp app(delay, true);
        return app.run_batch(iterations);
    }

    TopApp app(delay, false);
    return app.run();
}
