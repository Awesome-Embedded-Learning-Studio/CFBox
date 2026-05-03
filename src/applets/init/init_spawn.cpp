#include "init.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

namespace cfbox::init {

auto spawn_process(InitState& state, const InittabEntry& entry, bool respawn) -> pid_t {
    std::fflush(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "cfbox init: fork failed for '%s': %s\n",
                     entry.process.c_str(), std::strerror(errno));
        return -1;
    }

    if (pid == 0) {
        // Child: exec the command via sh -c
        // Create a new session for the child
        if (state.is_pid1) {
            setsid();
        }

        // Build argv for: /bin/sh -c "process"
        char shell[] = "/bin/sh";
        char dash_c[] = "-c";
        std::string cmd = entry.process;
        char* argv[] = { shell, dash_c, cmd.data(), nullptr };

        execv(shell, argv);
        // If execv fails, try /bin/cfbox sh
        std::fprintf(stderr, "cfbox init: exec failed for '%s': %s\n",
                     entry.process.c_str(), std::strerror(errno));
        _exit(127);
    }

    // Parent: track the child
    if (respawn) {
        SpawnedProcess sp;
        sp.pid = pid;
        sp.process = entry.process;
        sp.respawn = true;
        sp.last_spawn = std::chrono::steady_clock::now();
        sp.respawn_delay_ms = 2000;
        state.children.push_back(std::move(sp));
    }

    return pid;
}

auto reap_children(InitState& state) -> void {
    int status = 0;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Remove from children list if it was tracked
        for (auto it = state.children.begin(); it != state.children.end(); ++it) {
            if (it->pid == pid) {
                if (it->respawn && !state.shutting_down) {
                    // Mark for respawn (don't remove)
                    it->pid = 0;
                } else {
                    state.children.erase(it);
                }
                break;
            }
        }
    }
}

auto check_respawn(InitState& state) -> void {
    auto now = std::chrono::steady_clock::now();

    for (auto& child : state.children) {
        if (child.pid != 0 || !child.respawn || state.shutting_down) continue;

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - child.last_spawn).count();

        // If the process lived more than 60s, reset backoff
        if (elapsed_ms > 60000) {
            child.respawn_delay_ms = 2000;
        }

        // Check if enough time has passed for respawn
        auto wait_ms = child.respawn_delay_ms;
        if (elapsed_ms < wait_ms) continue;

        // Respawn
        InittabEntry fake_entry;
        fake_entry.process = child.process;

        auto new_pid = spawn_process(state, fake_entry, false);
        if (new_pid > 0) {
            child.pid = new_pid;
            child.last_spawn = now;
            // Exponential backoff, cap at 60s
            child.respawn_delay_ms = std::min(child.respawn_delay_ms * 2, 60000);
        }
    }
}

} // namespace cfbox::init
