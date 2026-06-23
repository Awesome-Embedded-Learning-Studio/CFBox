#include "init.hpp"

#include <cerrno>
#include <cfbox/error.hpp>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace cfbox::init {

auto spawn_process(InitState& state, const InittabEntry& entry, bool respawn, bool askfirst)
    -> pid_t {
    std::fflush(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        CFBOX_ERR("init", "fork failed for '%s': %s", entry.process.c_str(), std::strerror(errno));
        return -1;
    }

    if (pid == 0) {
        // Child: exec the command via sh -c
        // Create a new session for the child
        if (state.is_pid1) {
            setsid();
        }

        // inittab process may carry a leading '-' (e.g. askfirst "-/bin/sh"),
        // meaning it wants the controlling console; strip it so sh -c receives
        // a plain command rather than "-/bin/sh".
        std::string cmd = entry.process;
        if (!cmd.empty() && cmd[0] == '-') {
            cmd.erase(0, 1);
        }

        // askfirst: gate the exec behind a console keypress so boot logs are
        // not drowned by an immediate login prompt. EOF (no console wired up)
        // exits quietly; init respawns, re-prompting once a tty appears.
        if (askfirst) {
            static constexpr char prompt[] = "\nPlease press Enter to activate this console.";
            if (write(STDERR_FILENO, prompt, sizeof(prompt) - 1) < 0) {
                // best-effort prompt: a write failure does not block the
                // Enter wait below, so ignore it.
            }
            char c = 0;
            if (read(STDIN_FILENO, &c, 1) <= 0 || c != '\n') {
                _exit(0);
            }
        }

        // Build argv for: /bin/sh -c "process"
        char shell[] = "/bin/sh";
        char dash_c[] = "-c";
        char* argv[] = {shell, dash_c, cmd.data(), nullptr};

        execv(shell, argv);
        // If execv fails, try /bin/cfbox sh
        CFBOX_ERR("init", "exec failed for '%s': %s", entry.process.c_str(), std::strerror(errno));
        _exit(127);
    }

    // Parent: track the child
    if (respawn) {
        SpawnedProcess sp;
        sp.pid = pid;
        sp.process = entry.process;
        sp.respawn = true;
        sp.askfirst = askfirst;
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
        if (child.pid != 0 || !child.respawn || state.shutting_down)
            continue;

        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - child.last_spawn).count();

        // If the process lived more than 60s, reset backoff
        if (elapsed_ms > 60000) {
            child.respawn_delay_ms = 2000;
        }

        // Check if enough time has passed for respawn
        auto wait_ms = child.respawn_delay_ms;
        if (elapsed_ms < wait_ms)
            continue;

        // Respawn
        InittabEntry fake_entry;
        fake_entry.process = child.process;

        auto new_pid = spawn_process(state, fake_entry, false, child.askfirst);
        if (new_pid > 0) {
            child.pid = new_pid;
            child.last_spawn = now;
            // Exponential backoff, cap at 60s
            child.respawn_delay_ms = std::min(child.respawn_delay_ms * 2, 60000);
        }
    }
}

} // namespace cfbox::init
