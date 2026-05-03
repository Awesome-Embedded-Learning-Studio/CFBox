#include "init.hpp"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/mount.h>

namespace cfbox::init {

// Global state pointer for signal handlers
static InitState* g_state = nullptr;

static void sigchld_handler(int) {
    if (g_state) g_state->sigchld_received = true;
}

static void sigint_handler(int) {
    if (g_state) g_state->sigint_received = true;
}

static void sigterm_handler(int) {
    if (g_state) g_state->shutting_down = true;
}

auto install_signal_handlers(InitState& state) -> void {
    g_state = &state;

    struct sigaction sa {};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, nullptr);
}

auto do_shutdown(InitState& state, bool do_reboot) -> void {
    state.shutting_down = true;
    std::fflush(nullptr);

    std::printf("cfbox init: shutting down\n");
    std::fflush(stdout);

    // Run shutdown action entries
    for (const auto& e : state.entries) {
        if (e.action == "shutdown") {
            InittabEntry fake;
            fake.process = e.process;
            auto pid = spawn_process(state, fake, false);
            if (pid > 0) {
                int status = 0;
                waitpid(pid, &status, 0);
            }
        }
    }

    // Send SIGTERM to all processes
    kill(-1, SIGTERM);
    std::printf("cfbox init: sent SIGTERM to all processes\n");
    std::fflush(stdout);

    // Wait up to 2 seconds
    for (int i = 0; i < 20; ++i) {
        usleep(100000); // 100ms
    }

    // Send SIGKILL to remaining
    kill(-1, SIGKILL);
    std::printf("cfbox init: sent SIGKILL to remaining processes\n");
    std::fflush(stdout);
    usleep(100000);

    // Sync filesystems
    sync();

    // Unmount all filesystems (in reverse order)
    if (state.is_pid1) {
        umount("/dev");
        umount("/sys");
        umount("/proc");
    }

    if (state.is_pid1) {
        if (do_reboot) {
            reboot(RB_AUTOBOOT);
        } else {
            reboot(RB_POWER_OFF);
        }
    }
}

} // namespace cfbox::init
