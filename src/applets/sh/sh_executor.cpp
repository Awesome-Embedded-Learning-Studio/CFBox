#include "sh.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace cfbox::sh {

// Apply redirections, return saved fds for restoration
static auto apply_redirections(const std::vector<Redir>& redirs) -> std::vector<std::pair<int, int>> {
    std::vector<std::pair<int, int>> saved;

    for (const auto& r : redirs) {
        int target_fd = r.fd;
        if (target_fd < 0) target_fd = (r.type == Redir::Read || r.type == Redir::DupIn) ? 0 : 1;

        // Save original fd
        int saved_fd = ::dup(target_fd);
        if (saved_fd >= 0) {
            saved.emplace_back(target_fd, saved_fd);
        }

        switch (r.type) {
        case Redir::Read: {
            int fd = ::open(r.target.c_str(), O_RDONLY);
            if (fd < 0) {
                std::fprintf(stderr, "cfbox sh: %s: %s\n", r.target.c_str(), std::strerror(errno));
            } else {
                ::dup2(fd, target_fd);
                ::close(fd);
            }
            break;
        }
        case Redir::Write: {
            int fd = ::open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                std::fprintf(stderr, "cfbox sh: %s: %s\n", r.target.c_str(), std::strerror(errno));
            } else {
                ::dup2(fd, target_fd);
                ::close(fd);
            }
            break;
        }
        case Redir::Append: {
            int fd = ::open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                std::fprintf(stderr, "cfbox sh: %s: %s\n", r.target.c_str(), std::strerror(errno));
            } else {
                ::dup2(fd, target_fd);
                ::close(fd);
            }
            break;
        }
        case Redir::DupIn:
        case Redir::DupOut: {
            int src = std::atoi(r.target.c_str());
            if (src >= 0) ::dup2(src, target_fd);
            break;
        }
        }
    }

    return saved;
}

static auto restore_redirections(std::vector<std::pair<int, int>>& saved) -> void {
    for (auto& [target, saved_fd] : saved) {
        ::dup2(saved_fd, target);
        ::close(saved_fd);
    }
}

// Forward declare
static auto execute_pipeline(Pipeline& node, ShellState& state) -> int;

static auto execute_simple(SimpleCommand& cmd, ShellState& state) -> int {
    // Apply assignments
    for (auto& [name, value] : cmd.assigns) {
        state.set_var(name, value);
    }

    if (cmd.words.empty()) {
        // Assignment-only command
        return 0;
    }

    // Expand words
    auto expanded = expand_words(cmd.words, state);
    if (expanded.empty()) return 0;

    // Check builtin
    if (is_builtin(expanded[0])) {
        auto saved = apply_redirections(cmd.redirs);
        int rc = run_builtin(expanded[0], expanded, state);
        std::fflush(nullptr);
        restore_redirections(saved);
        return rc;
    }

    // External command: fork and exec
    pid_t pid = ::fork();
    if (pid < 0) {
        std::fprintf(stderr, "cfbox sh: fork failed: %s\n", std::strerror(errno));
        return 126;
    }

    if (pid == 0) {
        // Child process
        apply_redirections(cmd.redirs);

        // Build argv
        std::vector<char*> argv;
        for (auto& arg : expanded) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());
        std::fprintf(stderr, "cfbox sh: %s: command not found\n", argv[0]);
        ::_Exit(127);
    }

    // Parent: wait for child
    int status;
    ::waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

static auto execute_pipeline(Pipeline& node, ShellState& state) -> int {
    if (node.commands.size() == 1) {
        int rc = execute_command(node.commands[0], state);
        if (node.negated) rc = (rc == 0) ? 1 : 0;
        return rc;
    }

    // Multi-command pipeline: create pipes
    int n = static_cast<int>(node.commands.size());
    std::vector<int[2]> pipes(static_cast<std::size_t>(n - 1));
    for (int i = 0; i < n - 1; ++i) {
        if (::pipe(pipes[static_cast<std::size_t>(i)]) < 0) {
            std::fprintf(stderr, "cfbox sh: pipe failed\n");
            return 1;
        }
    }

    std::vector<pid_t> pids(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        pids[static_cast<std::size_t>(i)] = ::fork();
        if (pids[static_cast<std::size_t>(i)] < 0) {
            std::fprintf(stderr, "cfbox sh: fork failed\n");
            return 1;
        }

        if (pids[static_cast<std::size_t>(i)] == 0) {
            // Child i
            // Redirect stdin from previous pipe
            if (i > 0) {
                ::dup2(pipes[static_cast<std::size_t>(i - 1)][0], STDIN_FILENO);
            }
            // Redirect stdout to next pipe
            if (i < n - 1) {
                ::dup2(pipes[static_cast<std::size_t>(i)][1], STDOUT_FILENO);
            }
            // Close all pipe fds
            for (int j = 0; j < n - 1; ++j) {
                ::close(pipes[static_cast<std::size_t>(j)][0]);
                ::close(pipes[static_cast<std::size_t>(j)][1]);
            }

            // Execute command — if it's a simple command, exec directly
            auto& cmd = node.commands[static_cast<std::size_t>(i)];
            if (std::holds_alternative<SimpleCommand>(cmd)) {
                auto& sc = std::get<SimpleCommand>(cmd);
                apply_redirections(sc.redirs);
                auto expanded = expand_words(sc.words, state);
                if (expanded.empty()) ::_Exit(0);

                // Check builtin (run in-process for pipeline child)
                if (is_builtin(expanded[0])) {
                    int rc = run_builtin(expanded[0], expanded, state);
                    std::fflush(nullptr);
                    ::_Exit(rc);
                }

                std::vector<char*> argv;
                for (auto& arg : expanded) argv.push_back(arg.data());
                argv.push_back(nullptr);
                ::execvp(argv[0], argv.data());
                std::fprintf(stderr, "cfbox sh: %s: command not found\n", argv[0]);
                ::_Exit(127);
            } else {
                int rc = execute_command(cmd, state);
                std::fflush(nullptr);
                ::_Exit(rc);
            }
        }
    }

    // Parent: close all pipe fds
    for (int i = 0; i < n - 1; ++i) {
        ::close(pipes[static_cast<std::size_t>(i)][0]);
        ::close(pipes[static_cast<std::size_t>(i)][1]);
    }

    // Wait for all children
    int last_status = 0;
    for (int i = 0; i < n; ++i) {
        int status;
        ::waitpid(pids[static_cast<std::size_t>(i)], &status, 0);
        if (i == n - 1) {
            if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
        }
    }

    if (node.negated) last_status = (last_status == 0) ? 1 : 0;
    return last_status;
}

auto execute_command(Command& cmd, ShellState& state) -> int {
    return std::visit([&](auto& node) -> int {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, SimpleCommand>) {
            return execute_simple(node, state);
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<Pipeline>>) {
            return execute_pipeline(*node, state);
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<IfClause>>) {
            for (std::size_t i = 0; i < node->conditions.size(); ++i) {
                if (node->conditions[i]) {
                    int rc = execute(*node->conditions[i], state);
                    state.set_last_status(rc);
                    if (rc == 0 && node->bodies[i]) {
                        return execute(*node->bodies[i], state);
                    }
                }
            }
            // Else clause
            if (node->else_body) {
                return execute(*node->else_body, state);
            }
            return 0;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<WhileClause>>) {
            int rc = 0;
            while (true) {
                int cond_rc = node->condition ? execute(*node->condition, state) : 0;
                state.set_last_status(cond_rc);

                bool cond_match = node->is_until ? (cond_rc != 0) : (cond_rc == 0);
                if (!cond_match) break;

                if (node->body) {
                    rc = execute(*node->body, state);
                    state.set_last_status(rc);
                }

                if (state.should_exit) break;
                if (state.break_loop) { state.break_loop = false; break; }
                if (state.continue_loop) { state.continue_loop = false; continue; }
            }
            return rc;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<ForClause>>) {
            int rc = 0;
            auto words = node->words.empty()
                ? state.positional_params()
                : expand_words(node->words, state);

            for (auto& word : words) {
                state.set_var(node->var_name, word);
                if (node->body) {
                    rc = execute(*node->body, state);
                    state.set_last_status(rc);
                }
                if (state.should_exit) break;
                if (state.break_loop) { state.break_loop = false; break; }
                if (state.continue_loop) { state.continue_loop = false; continue; }
            }
            return rc;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<Subshell>>) {
            pid_t pid = ::fork();
            if (pid == 0) {
                int rc = node->body ? execute(*node->body, state) : 0;
                ::_Exit(rc);
            }
            int status;
            ::waitpid(pid, &status, 0);
            if (WIFEXITED(status)) return WEXITSTATUS(status);
            if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
            return 1;
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<BraceGroup>>) {
            return node->body ? execute(*node->body, state) : 0;
        }
        else {
            return 1;
        }
    }, cmd);
}

auto execute(AndOr& node, ShellState& state) -> int {
    int last_rc = 0;

    for (auto& [op, pipeline] : node.entries) {
        if (!pipeline) continue;

        // Check && / || condition
        if (op == AndOr::Op::And && last_rc != 0) continue;
        if (op == AndOr::Op::Or && last_rc == 0) continue;

        last_rc = execute_pipeline(*pipeline, state);
        state.set_last_status(last_rc);

        if (state.should_exit) break;
    }

    return last_rc;
}

} // namespace cfbox::sh
