#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <cfbox/error.hpp>
#include <cfbox/io.hpp>

namespace cfbox::proc {

// Cached system constants
inline auto clock_ticks_per_second() noexcept -> long {
    static long ticks = sysconf(_SC_CLK_TCK);
    return ticks;
}

inline auto page_size() noexcept -> long {
    static long ps = sysconf(_SC_PAGE_SIZE);
    return ps;
}

inline auto total_memory_kb() noexcept -> std::uint64_t {
    static std::uint64_t mem = static_cast<std::uint64_t>(sysconf(_SC_PHYS_PAGES)) *
                               static_cast<std::uint64_t>(sysconf(_SC_PAGE_SIZE)) / 1024;
    return mem;
}

// --- /proc/meminfo ---
struct MemInfo {
    std::uint64_t total = 0;
    std::uint64_t free = 0;
    std::uint64_t available = 0;
    std::uint64_t buffers = 0;
    std::uint64_t cached = 0;
    std::uint64_t swap_total = 0;
    std::uint64_t swap_free = 0;
    std::uint64_t shmem = 0;
    std::uint64_t s_reclaimable = 0;
};

inline auto read_meminfo() -> base::Result<MemInfo> {
    auto* f = std::fopen("/proc/meminfo", "r");
    if (!f)
        return std::unexpected(base::Error{1, "cannot open /proc/meminfo"});

    MemInfo mi{};
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        auto colon = std::strchr(line, ':');
        if (!colon)
            continue;
        *colon = '\0';
        auto* p = colon + 1;
        while (*p == ' ')
            ++p;
        std::uint64_t val = 0;
        std::sscanf(p, "%llu", reinterpret_cast<unsigned long long*>(&val));

        auto key = line;
        if (std::strcmp(key, "MemTotal") == 0)
            mi.total = val;
        else if (std::strcmp(key, "MemFree") == 0)
            mi.free = val;
        else if (std::strcmp(key, "MemAvailable") == 0)
            mi.available = val;
        else if (std::strcmp(key, "Buffers") == 0)
            mi.buffers = val;
        else if (std::strcmp(key, "Cached") == 0)
            mi.cached = val;
        else if (std::strcmp(key, "SwapTotal") == 0)
            mi.swap_total = val;
        else if (std::strcmp(key, "SwapFree") == 0)
            mi.swap_free = val;
        else if (std::strcmp(key, "Shmem") == 0)
            mi.shmem = val;
        else if (std::strcmp(key, "SReclaimable") == 0)
            mi.s_reclaimable = val;
    }
    std::fclose(f);

    // Adjust cached to include SReclaimable (like procps does)
    mi.cached += mi.s_reclaimable;

    return mi;
}

// --- /proc/stat (CPU) ---
struct CpuStats {
    std::uint64_t user = 0, nice = 0, system = 0, idle = 0;
    std::uint64_t iowait = 0, irq = 0, softirq = 0, steal = 0;
    auto total() const noexcept -> std::uint64_t {
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }
    auto idle_time() const noexcept -> std::uint64_t { return idle + iowait; }
};

inline auto read_cpu_stats() -> base::Result<CpuStats> {
    auto* f = std::fopen("/proc/stat", "r");
    if (!f)
        return std::unexpected(base::Error{1, "cannot open /proc/stat"});

    char line[512];
    if (!std::fgets(line, sizeof(line), f) || std::strncmp(line, "cpu ", 4) != 0) {
        std::fclose(f);
        return std::unexpected(base::Error{1, "unexpected /proc/stat format"});
    }
    std::fclose(f);

    CpuStats cs{};
    // "cpu  user nice system idle iowait irq softirq steal [guest guest_nice]"
    std::sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
                reinterpret_cast<unsigned long long*>(&cs.user),
                reinterpret_cast<unsigned long long*>(&cs.nice),
                reinterpret_cast<unsigned long long*>(&cs.system),
                reinterpret_cast<unsigned long long*>(&cs.idle),
                reinterpret_cast<unsigned long long*>(&cs.iowait),
                reinterpret_cast<unsigned long long*>(&cs.irq),
                reinterpret_cast<unsigned long long*>(&cs.softirq),
                reinterpret_cast<unsigned long long*>(&cs.steal));
    return cs;
}

// --- /proc/[pid]/stat ---
struct ProcessInfo {
    pid_t pid = 0;
    std::string comm;
    char state = '?';
    pid_t ppid = 0;
    int priority = 0;
    int nice_val = 0;
    std::uint64_t vsize = 0;      // bytes
    std::uint64_t rss = 0;        // pages
    std::uint64_t utime = 0;      // clock ticks
    std::uint64_t stime = 0;      // clock ticks
    std::uint64_t start_time = 0; // clock ticks since boot
    uid_t uid = static_cast<uid_t>(-1);
    gid_t gid = static_cast<gid_t>(-1);
    std::vector<std::string> cmdline;
    std::string tty;
};

namespace detail {

inline auto parse_uid_gid(std::string_view path, uid_t& uid, gid_t& gid) -> void {
    auto p = std::string(path);
    auto* f = std::fopen(p.c_str(), "r");
    if (!f)
        return;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "Uid:", 4) == 0) {
            std::sscanf(line + 4, " %u", &uid);
        } else if (std::strncmp(line, "Gid:", 4) == 0) {
            std::sscanf(line + 4, " %u", &gid);
        }
    }
    std::fclose(f);
}

inline auto parse_cmdline(std::string_view path) -> std::vector<std::string> {
    auto p = std::string(path);
    auto* f = std::fopen(p.c_str(), "rb");
    if (!f)
        return {};

    // Read entire file
    std::fseek(f, 0, SEEK_END);
    auto sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(f);
        return {};
    }

    std::string data(static_cast<std::size_t>(sz), '\0');
    auto nread = std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    data.resize(nread);

    if (data.empty())
        return {};

    std::vector<std::string> args;
    std::string::size_type start = 0;
    for (std::string::size_type i = 0; i <= data.size(); ++i) {
        if (i == data.size() || data[i] == '\0') {
            if (i > start)
                args.emplace_back(data.substr(start, i - start));
            start = i + 1;
        }
    }
    return args;
}

inline auto tty_from_dev(std::uint64_t tty_nr) -> std::string {
    int major = static_cast<int>(tty_nr >> 8);
    int minor = static_cast<int>(tty_nr & 0xFF);
    if (major == 4) {
        if (minor < 64)
            return "tty" + std::to_string(minor);
        return "pts/" + std::to_string(minor - 64 > 255 ? minor : minor - 64);
    }
    if (major == 136)
        return "pts/" + std::to_string(minor);
    if (major == 5 && minor == 0)
        return "tty";
    return "?";
}

} // namespace detail

inline auto read_process(pid_t pid) -> base::Result<ProcessInfo> {
    ProcessInfo pi;
    pi.pid = pid;

    // Parse /proc/[pid]/stat
    auto stat_path = "/proc/" + std::to_string(pid) + "/stat";
    auto content_result = cfbox::io::read_all(stat_path);
    if (!content_result)
        return std::unexpected(base::Error{1, "cannot read " + stat_path});
    auto& content = *content_result;

    // comm is in parentheses and may contain spaces — find last ')'
    auto open_paren = content.find('(');
    auto close_paren = content.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos ||
        close_paren <= open_paren)
        return std::unexpected(base::Error{1, "unexpected /proc/[pid]/stat format"});

    pi.comm = content.substr(open_paren + 1, close_paren - open_paren - 1);

    // Fields after close paren: state ppid pgrp session tty_nr tpgid ...
    // Field 3 (state) is right after ') '
    const char* p = content.c_str() + close_paren + 2;

    // field 3: state (char)
    pi.state = *p;
    ++p; // skip state char
    if (*p == ' ')
        ++p;

    // field 4: ppid
    pi.ppid = static_cast<pid_t>(std::strtoull(p, nullptr, 10));
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 5: pgrp (skip)
    std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 6: session (skip)
    std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 7: tty_nr
    std::uint64_t tty_nr = std::strtoull(p, nullptr, 10);
    pi.tty = detail::tty_from_dev(tty_nr);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 8: tpgid (skip)
    std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // fields 9-13: flags, minflt, cminflt, majflt, cmajflt (skip 5 fields)
    for (int i = 0; i < 5; ++i) {
        std::strtoull(p, nullptr, 10);
        while (*p && *p != ' ')
            ++p;
        if (*p == ' ')
            ++p;
    }

    // field 14: utime
    pi.utime = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 15: stime
    pi.stime = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 16-17: cutime, cstime (skip)
    for (int i = 0; i < 2; ++i) {
        std::strtoull(p, nullptr, 10);
        while (*p && *p != ' ')
            ++p;
        if (*p == ' ')
            ++p;
    }

    // field 18: priority
    pi.priority = static_cast<int>(std::strtol(p, nullptr, 10));
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 19: nice
    pi.nice_val = static_cast<int>(std::strtol(p, nullptr, 10));
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // fields 20-21: num_threads, itrealvalue (skip)
    for (int i = 0; i < 2; ++i) {
        std::strtoull(p, nullptr, 10);
        while (*p && *p != ' ')
            ++p;
        if (*p == ' ')
            ++p;
    }

    // field 22: starttime
    pi.start_time = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 23: vsize (bytes)
    pi.vsize = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ')
        ++p;
    if (*p == ' ')
        ++p;

    // field 24: rss (pages)
    pi.rss = std::strtoull(p, nullptr, 10);

    // uid/gid from /proc/[pid]/status
    auto status_path = "/proc/" + std::to_string(pid) + "/status";
    detail::parse_uid_gid(status_path, pi.uid, pi.gid);

    // cmdline from /proc/[pid]/cmdline
    auto cmdline_path = "/proc/" + std::to_string(pid) + "/cmdline";
    pi.cmdline = detail::parse_cmdline(cmdline_path);

    return pi;
}

inline auto read_all_processes() -> base::Result<std::vector<ProcessInfo>> {
    std::vector<ProcessInfo> procs;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/proc", ec)) {
        if (!entry.is_directory())
            continue;
        auto name = entry.path().filename().string();
        // Check if directory name is numeric (PID)
        if (name.empty() || name[0] < '0' || name[0] > '9')
            continue;
        pid_t pid = static_cast<pid_t>(std::strtol(name.c_str(), nullptr, 10));
        auto result = read_process(pid);
        if (result)
            procs.push_back(std::move(*result));
    }
    return procs;
}

// --- /proc/loadavg ---
struct LoadAvg {
    double avg1 = 0, avg5 = 0, avg15 = 0;
    int running = 0, total = 0;
    pid_t last_pid = 0;
};

inline auto read_loadavg() -> base::Result<LoadAvg> {
    auto content_result = cfbox::io::read_all("/proc/loadavg");
    if (!content_result)
        return std::unexpected(base::Error{1, "cannot read /proc/loadavg"});

    LoadAvg la{};
    std::sscanf(content_result->c_str(), "%lf %lf %lf %d/%d %d", &la.avg1, &la.avg5, &la.avg15,
                &la.running, &la.total, reinterpret_cast<int*>(&la.last_pid));
    return la;
}

// --- /proc/uptime ---
inline auto read_uptime() -> base::Result<std::pair<double, double>> {
    auto content_result = cfbox::io::read_all("/proc/uptime");
    if (!content_result)
        return std::unexpected(base::Error{1, "cannot read /proc/uptime"});

    double up = 0, idle = 0;
    std::sscanf(content_result->c_str(), "%lf %lf", &up, &idle);
    return std::make_pair(up, idle);
}

// --- /proc/version ---
inline auto read_version() -> base::Result<std::string> {
    auto content_result = cfbox::io::read_all("/proc/version");
    if (!content_result)
        return std::unexpected(base::Error{1, "cannot read /proc/version"});
    auto& content = *content_result;
    // Trim trailing newline
    while (!content.empty() && content.back() == '\n')
        content.pop_back();
    return content;
}

// --- /proc/mounts ---
struct MountEntry {
    std::string device;
    std::string mountpoint;
    std::string fstype;
    std::string options;
};

inline auto read_mounts() -> base::Result<std::vector<MountEntry>> {
    auto* f = std::fopen("/proc/mounts", "r");
    if (!f)
        return std::unexpected(base::Error{1, "cannot open /proc/mounts"});

    std::vector<MountEntry> mounts;
    char line[1024];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '\n')
            continue;
        MountEntry me;
        char dev[256], mp[256], fs[256], opts[256];
        if (std::sscanf(line, "%255s %255s %255s %255s", dev, mp, fs, opts) == 4) {
            me.device = dev;
            me.mountpoint = mp;
            me.fstype = fs;
            me.options = opts;
            mounts.push_back(std::move(me));
        }
    }
    std::fclose(f);
    return mounts;
}

// --- /proc/diskstats ---
struct DiskStat {
    std::string device;
    std::uint64_t reads = 0, reads_merged = 0, sectors_read = 0, ms_reading = 0;
    std::uint64_t writes = 0, writes_merged = 0, sectors_written = 0, ms_writing = 0;
    std::uint64_t ios_in_progress = 0, ms_ios = 0, weighted_ms_ios = 0;
};

inline auto read_diskstats() -> base::Result<std::vector<DiskStat>> {
    auto* f = std::fopen("/proc/diskstats", "r");
    if (!f)
        return std::unexpected(base::Error{1, "cannot open /proc/diskstats"});

    std::vector<DiskStat> stats;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '\n')
            continue;
        DiskStat ds;
        char dev[64];
        auto n =
            std::sscanf(line, "%*d %*d %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                        dev, reinterpret_cast<unsigned long long*>(&ds.reads),
                        reinterpret_cast<unsigned long long*>(&ds.reads_merged),
                        reinterpret_cast<unsigned long long*>(&ds.sectors_read),
                        reinterpret_cast<unsigned long long*>(&ds.ms_reading),
                        reinterpret_cast<unsigned long long*>(&ds.writes),
                        reinterpret_cast<unsigned long long*>(&ds.writes_merged),
                        reinterpret_cast<unsigned long long*>(&ds.sectors_written),
                        reinterpret_cast<unsigned long long*>(&ds.ms_writing),
                        reinterpret_cast<unsigned long long*>(&ds.ios_in_progress),
                        reinterpret_cast<unsigned long long*>(&ds.ms_ios),
                        reinterpret_cast<unsigned long long*>(&ds.weighted_ms_ios));
        if (n >= 1) {
            ds.device = dev;
            stats.push_back(std::move(ds));
        }
    }
    std::fclose(f);
    return stats;
}

// --- /proc/partitions ---
inline auto read_partitions() -> base::Result<std::vector<std::string>> {
    auto* f = std::fopen("/proc/partitions", "r");
    if (!f)
        return std::unexpected(base::Error{1, "cannot open /proc/partitions"});

    std::vector<std::string> parts;
    char line[256];
    // Skip header line
    std::fgets(line, sizeof(line), f);
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '\n')
            continue;
        int major, minor;
        std::uint64_t blocks;
        char name[64];
        if (std::sscanf(line, "%d %d %llu %63s", &major, &minor,
                        reinterpret_cast<unsigned long long*>(&blocks), name) >= 4) {
            parts.emplace_back(name);
        }
    }
    std::fclose(f);
    return parts;
}

} // namespace cfbox::proc
