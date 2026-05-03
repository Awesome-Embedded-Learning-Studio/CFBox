#include <gtest/gtest.h>
#include <cfbox/proc.hpp>

TEST(ProcTest, ReadMeminfo) {
    auto result = cfbox::proc::read_meminfo();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    const auto& mi = *result;
    EXPECT_GT(mi.total, 0u);
    EXPECT_GT(mi.free, 0u);
    EXPECT_LE(mi.free, mi.total);
    EXPECT_LE(mi.available, mi.total);
    EXPECT_LE(mi.swap_free, mi.swap_total);
}

TEST(ProcTest, ReadCpuStats) {
    auto result = cfbox::proc::read_cpu_stats();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    const auto& cs = *result;
    EXPECT_GT(cs.total(), 0u);
    EXPECT_GT(cs.idle_time(), 0u);
    EXPECT_LE(cs.idle_time(), cs.total());
}

TEST(ProcTest, ClockTicksPerSecond) {
    auto ticks = cfbox::proc::clock_ticks_per_second();
    EXPECT_GT(ticks, 0);
}

TEST(ProcTest, TotalMemoryKb) {
    auto mem = cfbox::proc::total_memory_kb();
    EXPECT_GT(mem, 0u);
}

TEST(ProcTest, ReadLoadavg) {
    auto result = cfbox::proc::read_loadavg();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    const auto& la = *result;
    EXPECT_GE(la.avg1, 0.0);
    EXPECT_GE(la.avg5, 0.0);
    EXPECT_GE(la.avg15, 0.0);
    EXPECT_GE(la.running, 0);
    EXPECT_GE(la.total, 0);
}

TEST(ProcTest, ReadUptime) {
    auto result = cfbox::proc::read_uptime();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    auto [up, idle] = *result;
    EXPECT_GT(up, 0.0);
    EXPECT_GE(idle, 0.0);
}

TEST(ProcTest, ReadVersion) {
    auto result = cfbox::proc::read_version();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    EXPECT_FALSE(result->empty());
    // Should contain "Linux"
    EXPECT_NE(result->find("Linux"), std::string::npos)
        << "version: " << *result;
}

TEST(ProcTest, ReadMounts) {
    auto result = cfbox::proc::read_mounts();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    EXPECT_FALSE(result->empty());
    // At least /proc should be mounted
    bool found_proc = false;
    for (const auto& m : *result) {
        if (m.mountpoint == "/proc") { found_proc = true; break; }
    }
    EXPECT_TRUE(found_proc) << "/proc not found in mount list";
}

TEST(ProcTest, ReadProcessSelf) {
    auto result = cfbox::proc::read_process(getpid());
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    const auto& pi = *result;
    EXPECT_EQ(pi.pid, getpid());
    EXPECT_EQ(pi.uid, getuid());
    EXPECT_EQ(pi.gid, getgid());
    EXPECT_NE(pi.state, '?');
}

TEST(ProcTest, ReadAllProcesses) {
    auto result = cfbox::proc::read_all_processes();
    ASSERT_TRUE(result.has_value()) << result.error().msg;
    EXPECT_FALSE(result->empty());
    // Self should be in the list
    bool found_self = false;
    for (const auto& p : *result) {
        if (p.pid == getpid()) { found_self = true; break; }
    }
    EXPECT_TRUE(found_self);
}

TEST(ProcTest, ReadDiskstats) {
    auto result = cfbox::proc::read_diskstats();
    // May be empty in some environments (containers), just verify it parses
    if (result.has_value()) {
        for (const auto& ds : *result) {
            EXPECT_FALSE(ds.device.empty());
        }
    }
}

TEST(ProcTest, ReadPartitions) {
    auto result = cfbox::proc::read_partitions();
    // May be empty in some environments, just verify no error
    EXPECT_TRUE(result.has_value()) << result.error().msg;
}
