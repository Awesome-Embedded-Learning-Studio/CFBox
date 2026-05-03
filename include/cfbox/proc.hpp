#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

#include <cfbox/error.hpp>

namespace cfbox::proc {

// Cached system constants
inline auto clock_ticks_per_second() -> long {
    static long ticks = sysconf(_SC_CLK_TCK);
    return ticks;
}

inline auto page_size() -> long {
    static long ps = sysconf(_SC_PAGE_SIZE);
    return ps;
}

inline auto total_memory_kb() -> std::uint64_t {
    static std::uint64_t mem = static_cast<std::uint64_t>(sysconf(_SC_PHYS_PAGES))
                               * static_cast<std::uint64_t>(sysconf(_SC_PAGE_SIZE)) / 1024;
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
    std::ifstream f("/proc/meminfo");
    if (!f) return std::unexpected(base::Error{1, "cannot open /proc/meminfo"});

    MemInfo mi{};
    std::string line;
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto key = line.substr(0, colon);
        // Parse number after colon
        auto p = line.c_str() + colon + 1;
        while (*p == ' ') ++p;
        std::uint64_t val = 0;
        std::sscanf(p, "%llu", reinterpret_cast<unsigned long long*>(&val));

        if (key == "MemTotal")          mi.total = val;
        else if (key == "MemFree")      mi.free = val;
        else if (key == "MemAvailable") mi.available = val;
        else if (key == "Buffers")      mi.buffers = val;
        else if (key == "Cached")       mi.cached = val;
        else if (key == "SwapTotal")    mi.swap_total = val;
        else if (key == "SwapFree")     mi.swap_free = val;
        else if (key == "Shmem")        mi.shmem = val;
        else if (key == "SReclaimable") mi.s_reclaimable = val;
    }

    // Adjust cached to include SReclaimable (like procps does)
    mi.cached += mi.s_reclaimable;

    return mi;
}

// --- /proc/stat (CPU) ---
struct CpuStats {
    std::uint64_t user = 0, nice = 0, system = 0, idle = 0;
    std::uint64_t iowait = 0, irq = 0, softirq = 0, steal = 0;
    auto total() const -> std::uint64_t {
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }
    auto idle_time() const -> std::uint64_t {
        return idle + iowait;
    }
};

inline auto read_cpu_stats() -> base::Result<CpuStats> {
    std::ifstream f("/proc/stat");
    if (!f) return std::unexpected(base::Error{1, "cannot open /proc/stat"});

    std::string line;
    if (!std::getline(f, line) || line.substr(0, 4) != "cpu ")
        return std::unexpected(base::Error{1, "unexpected /proc/stat format"});

    CpuStats cs{};
    // "cpu  user nice system idle iowait irq softirq steal [guest guest_nice]"
    std::istringstream iss(line.substr(5));
    iss >> cs.user >> cs.nice >> cs.system >> cs.idle
        >> cs.iowait >> cs.irq >> cs.softirq >> cs.steal;
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
    std::uint64_t vsize = 0;    // bytes
    std::uint64_t rss = 0;      // pages
    std::uint64_t utime = 0;    // clock ticks
    std::uint64_t stime = 0;    // clock ticks
    std::uint64_t start_time = 0; // clock ticks since boot
    uid_t uid = static_cast<uid_t>(-1);
    gid_t gid = static_cast<gid_t>(-1);
    std::vector<std::string> cmdline;
    std::string tty;
};

namespace detail {

inline auto read_file_str(std::string_view path) -> std::string {
    auto p = std::string(path);
    std::ifstream f{p};
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline auto parse_uid_gid(std::string_view path, uid_t& uid, gid_t& gid) -> void {
    auto p = std::string(path);
    std::ifstream f{p};
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with("Uid:")) {
            std::sscanf(line.c_str() + 4, " %u", &uid);
        } else if (line.starts_with("Gid:")) {
            std::sscanf(line.c_str() + 4, " %u", &gid);
        }
    }
}

inline auto parse_cmdline(std::string_view path) -> std::vector<std::string> {
    auto p = std::string(path);
    std::ifstream f{p, std::ios::binary};
    if (!f) return {};
    std::string data((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    if (data.empty()) return {};

    std::vector<std::string> args;
    std::string::size_type start = 0;
    for (std::string::size_type i = 0; i <= data.size(); ++i) {
        if (i == data.size() || data[i] == '\0') {
            if (i > start) args.emplace_back(data.substr(start, i - start));
            start = i + 1;
        }
    }
    return args;
}

inline auto tty_from_dev(std::uint64_t tty_nr) -> std::string {
    int major = static_cast<int>(tty_nr >> 8);
    int minor = static_cast<int>(tty_nr & 0xFF);
    if (major == 4) {
        if (minor < 64) return "tty" + std::to_string(minor);
        return "pts/" + std::to_string(minor - 64 > 255 ? minor : minor - 64);
    }
    if (major == 136) return "pts/" + std::to_string(minor);
    if (major == 5 && minor == 0) return "tty";
    return "?";
}

} // namespace detail

inline auto read_process(pid_t pid) -> base::Result<ProcessInfo> {
    ProcessInfo pi;
    pi.pid = pid;

    // Parse /proc/[pid]/stat
    auto stat_path = "/proc/" + std::to_string(pid) + "/stat";
    auto content = detail::read_file_str(stat_path);
    if (content.empty())
        return std::unexpected(base::Error{1, "cannot read " + stat_path});

    // comm is in parentheses and may contain spaces — find last ')'
    auto open_paren = content.find('(');
    auto close_paren = content.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos || close_paren <= open_paren)
        return std::unexpected(base::Error{1, "unexpected /proc/[pid]/stat format"});

    pi.comm = content.substr(open_paren + 1, close_paren - open_paren - 1);

    // Fields after close paren: state ppid pgrp session tty_nr tpgid ...
    // Field 3 (state) is right after ') '
    const char* p = content.c_str() + close_paren + 2;

    // field 3: state (char)
    pi.state = *p;
    ++p; // skip state char
    if (*p == ' ') ++p;

    // field 4: ppid
    pi.ppid = static_cast<pid_t>(std::strtoull(p, nullptr, 10));
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 5: pgrp (skip)
    std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 6: session (skip)
    std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 7: tty_nr
    std::uint64_t tty_nr = std::strtoull(p, nullptr, 10);
    pi.tty = detail::tty_from_dev(tty_nr);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 8: tpgid (skip)
    std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // fields 9-13: flags, minflt, cminflt, majflt, cmajflt (skip 5 fields)
    for (int i = 0; i < 5; ++i) {
        std::strtoull(p, nullptr, 10);
        while (*p && *p != ' ') ++p;
        if (*p == ' ') ++p;
    }

    // field 14: utime
    pi.utime = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 15: stime
    pi.stime = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 16-17: cutime, cstime (skip)
    for (int i = 0; i < 2; ++i) {
        std::strtoull(p, nullptr, 10);
        while (*p && *p != ' ') ++p;
        if (*p == ' ') ++p;
    }

    // field 18: priority
    pi.priority = static_cast<int>(std::strtol(p, nullptr, 10));
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 19: nice
    pi.nice_val = static_cast<int>(std::strtol(p, nullptr, 10));
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // fields 20-21: num_threads, itrealvalue (skip)
    for (int i = 0; i < 2; ++i) {
        std::strtoull(p, nullptr, 10);
        while (*p && *p != ' ') ++p;
        if (*p == ' ') ++p;
    }

    // field 22: starttime
    pi.start_time = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

    // field 23: vsize (bytes)
    pi.vsize = std::strtoull(p, nullptr, 10);
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;

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
        if (!entry.is_directory()) continue;
        auto name = entry.path().filename().string();
        // Check if directory name is numeric (PID)
        if (name.empty() || name[0] < '0' || name[0] > '9') continue;
        pid_t pid = static_cast<pid_t>(std::stoi(name));
        auto result = read_process(pid);
        if (result) procs.push_back(std::move(*result));
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
    auto content = detail::read_file_str("/proc/loadavg");
    if (content.empty())
        return std::unexpected(base::Error{1, "cannot read /proc/loadavg"});

    LoadAvg la{};
    std::sscanf(content.c_str(), "%lf %lf %lf %d/%d %d",
                &la.avg1, &la.avg5, &la.avg15, &la.running, &la.total, reinterpret_cast<int*>(&la.last_pid));
    return la;
}

// --- /proc/uptime ---
inline auto read_uptime() -> base::Result<std::pair<double, double>> {
    auto content = detail::read_file_str("/proc/uptime");
    if (content.empty())
        return std::unexpected(base::Error{1, "cannot read /proc/uptime"});

    double up = 0, idle = 0;
    std::sscanf(content.c_str(), "%lf %lf", &up, &idle);
    return std::make_pair(up, idle);
}

// --- /proc/version ---
inline auto read_version() -> base::Result<std::string> {
    auto content = detail::read_file_str("/proc/version");
    if (content.empty())
        return std::unexpected(base::Error{1, "cannot read /proc/version"});
    // Trim trailing newline
    while (!content.empty() && content.back() == '\n') content.pop_back();
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
    std::ifstream f("/proc/mounts");
    if (!f) return std::unexpected(base::Error{1, "cannot open /proc/mounts"});

    std::vector<MountEntry> mounts;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        // device mountpoint fstype options freq passno
        MountEntry me;
        std::istringstream iss(line);
        iss >> me.device >> me.mountpoint >> me.fstype >> me.options;
        if (!me.device.empty()) mounts.push_back(std::move(me));
    }
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
    std::ifstream f("/proc/diskstats");
    if (!f) return std::unexpected(base::Error{1, "cannot open /proc/diskstats"});

    std::vector<DiskStat> stats;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        DiskStat ds;
        std::istringstream iss(line);
        std::string maj_str, min_str;
        iss >> maj_str >> min_str >> ds.device
            >> ds.reads >> ds.reads_merged >> ds.sectors_read >> ds.ms_reading
            >> ds.writes >> ds.writes_merged >> ds.sectors_written >> ds.ms_writing
            >> ds.ios_in_progress >> ds.ms_ios >> ds.weighted_ms_ios;

        if (!ds.device.empty()) stats.push_back(std::move(ds));
    }
    return stats;
}

// --- /proc/partitions ---
inline auto read_partitions() -> base::Result<std::vector<std::string>> {
    std::ifstream f("/proc/partitions");
    if (!f) return std::unexpected(base::Error{1, "cannot open /proc/partitions"});

    std::vector<std::string> parts;
    std::string line;
    // Skip header line
    std::getline(f, line);
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        int major, minor;
        std::uint64_t blocks;
        std::string name;
        iss >> major >> minor >> blocks >> name;
        if (!name.empty()) parts.push_back(std::move(name));
    }
    return parts;
}

} // namespace cfbox::proc
