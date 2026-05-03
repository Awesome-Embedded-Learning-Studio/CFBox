#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <sys/types.h>
#include <sys/wait.h>

#include <cfbox/help.hpp>
#include <cfbox/applet_config.hpp>

namespace cfbox::init {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "init",
    .version = CFBOX_VERSION_STRING,
    .one_line = "system init (PID 1) with inittab support",
    .usage   = "init",
    .options = "",
    .extra   = "If /etc/inittab exists, runs full init system.\n"
               "Otherwise falls back to boot-test mode for QEMU.",
};

// inittab entry: id:runlevels:action:process
struct InittabEntry {
    std::string id;           // 1-4 char identifier
    std::string runlevels;    // which runlevels (empty = all)
    std::string action;       // sysinit, boot, once, respawn, wait, ctrlaltdel, shutdown
    std::string process;      // command to run
};

enum class RunLevel {
    SysInit,
    Boot,
    Single,
    MultiUser,  // runlevel 2-5
    Shutdown,
};

// Tracks a spawned child process and its inittab entry
struct SpawnedProcess {
    pid_t pid = 0;
    std::string process;       // command
    bool respawn = false;      // true if this is a respawn entry
    std::chrono::steady_clock::time_point last_spawn;
    int respawn_delay_ms = 2000; // initial backoff delay
};

struct InitState {
    bool is_pid1 = false;
    RunLevel runlevel = RunLevel::SysInit;
    std::vector<InittabEntry> entries;
    std::vector<SpawnedProcess> children;
    bool shutting_down = false;
    bool sigchld_received = false;
    bool sigint_received = false;
};

// --- Module interfaces ---

// init_inittab.cpp
auto parse_inittab(const std::string& path) -> std::vector<InittabEntry>;
auto parse_inittab_line(std::string_view line) -> InittabEntry;

// init_runlevel.cpp
auto run_sysinit(InitState& state) -> void;
auto run_boot_entries(InitState& state) -> void;
auto run_level_entries(InitState& state, const std::string& level) -> void;

// init_spawn.cpp
auto spawn_process(InitState& state, const InittabEntry& entry, bool respawn) -> pid_t;
auto reap_children(InitState& state) -> void;
auto check_respawn(InitState& state) -> void;

// init_shutdown.cpp
auto do_shutdown(InitState& state, bool reboot) -> void;
auto install_signal_handlers(InitState& state) -> void;

// Smoke test mode (QEMU compatibility)
auto run_smoke_tests(InitState& state) -> void;

} // namespace cfbox::init
