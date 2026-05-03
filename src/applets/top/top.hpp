#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#include <cfbox/proc.hpp>

namespace top {

enum class SortField {
    Cpu, Mem, Pid, Time
};

struct ProcEntry {
    pid_t pid = 0;
    std::string user;
    int priority = 0;
    int nice_val = 0;
    std::uint64_t vsize = 0;     // bytes
    std::uint64_t rss_kb = 0;    // KB
    char state = '?';
    double cpu_pct = 0.0;
    double mem_pct = 0.0;
    std::uint64_t total_time = 0; // ticks
    std::string command;
    std::string tty;
};

inline auto uid_to_name(uid_t uid) -> std::string {
    // Simple approach: just return the uid as string
    // Full implementation would read /etc/passwd
    return std::to_string(uid);
}

inline auto build_entries(std::uint64_t total_mem_kb) -> std::vector<ProcEntry> {
    auto result = cfbox::proc::read_all_processes();
    if (!result) return {};

    std::vector<ProcEntry> entries;
    entries.reserve(result->size());

    for (const auto& p : *result) {
        ProcEntry e;
        e.pid = p.pid;
        e.user = uid_to_name(p.uid);
        e.priority = p.priority;
        e.nice_val = p.nice_val;
        e.vsize = p.vsize;
        e.rss_kb = p.rss * static_cast<std::uint64_t>(cfbox::proc::page_size()) / 1024;
        e.state = p.state;
        e.total_time = p.utime + p.stime;
        e.tty = p.tty;

        // Build command string
        if (!p.cmdline.empty()) {
            e.command = p.cmdline[0];
            // Truncate long commands
            if (e.command.size() > 30) {
                e.command.resize(27);
                e.command += "...";
            }
        } else {
            e.command = "[" + p.comm + "]";
        }

        // CPU% and MEM% will be calculated by sort logic
        if (total_mem_kb > 0) {
            e.mem_pct = 100.0 * static_cast<double>(e.rss_kb) / static_cast<double>(total_mem_kb);
        }

        entries.push_back(std::move(e));
    }
    return entries;
}

inline auto sort_entries(std::vector<ProcEntry>& entries, SortField field) -> void {
    switch (field) {
        case SortField::Cpu:
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.cpu_pct > b.cpu_pct; });
            break;
        case SortField::Mem:
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.rss_kb > b.rss_kb; });
            break;
        case SortField::Pid:
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.pid < b.pid; });
            break;
        case SortField::Time:
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.total_time > b.total_time; });
            break;
    }
}

} // namespace top
