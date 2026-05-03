#include "init.hpp"

#include <sys/mount.h>
#include <sys/wait.h>

namespace cfbox::init {

auto run_sysinit(InitState& state) -> void {
    std::fflush(nullptr);

    // Mount essential filesystems if PID 1
    if (state.is_pid1) {
        mount("proc",     "/proc", "proc",     0, nullptr);
        mount("sysfs",    "/sys",  "sysfs",    0, nullptr);
        mount("devtmpfs", "/dev",  "devtmpfs", 0, nullptr);
    }

    // Run sysinit entries (sequentially, wait for each)
    for (const auto& e : state.entries) {
        if (e.action == "sysinit") {
            auto pid = spawn_process(state, e, false);
            if (pid > 0) {
                int status = 0;
                waitpid(pid, &status, 0);
            }
        }
    }

    state.runlevel = RunLevel::Boot;
}

auto run_boot_entries(InitState& state) -> void {
    for (const auto& e : state.entries) {
        if (e.action == "boot" || e.action == "bootwait") {
            auto pid = spawn_process(state, e, false);
            if (pid > 0 && e.action == "bootwait") {
                int status = 0;
                waitpid(pid, &status, 0);
            }
        }
    }

    state.runlevel = RunLevel::MultiUser;
}

auto run_level_entries(InitState& state, const std::string& level) -> void {
    for (const auto& e : state.entries) {
        // Check if entry applies to current runlevel
        if (!e.runlevels.empty() && e.runlevels.find(level[0]) == std::string::npos)
            continue;

        bool should_respawn = (e.action == "respawn");
        bool should_wait = (e.action == "wait");
        bool is_once = (e.action == "once");

        if (should_respawn || is_once || should_wait) {
            auto pid = spawn_process(state, e, should_respawn);
            if (pid > 0 && should_wait) {
                int status = 0;
                waitpid(pid, &status, 0);
            }
        }
    }
}

} // namespace cfbox::init
