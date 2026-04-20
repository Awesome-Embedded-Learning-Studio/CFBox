#include <cstdio>
#include <cstring>
#include <csignal>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "timeout",
    .version = CFBOX_VERSION_STRING,
    .one_line = "run a command with a time limit",
    .usage   = "timeout DURATION COMMAND [ARGS]...",
    .options = "  -s SIG   send SIG on timeout (default: TERM)\n"
               "  -k DUR   also send KILL after DUR",
    .extra   = "DURATION supports floating point (e.g. 2.5 for 2.5 seconds).",
};
} // namespace

auto timeout_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', true, "signal"},
        cfbox::args::OptSpec{'k', true, "kill-after"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox timeout: missing operand\n");
        return 1;
    }

    char* end = nullptr;
    double duration = std::strtod(std::string{pos[0]}.c_str(), &end);
    if (end == std::string{pos[0]}.c_str() || duration <= 0) {
        std::fprintf(stderr, "cfbox timeout: invalid duration '%.*s'\n",
                     static_cast<int>(pos[0].size()), pos[0].data());
        return 1;
    }

    int sig = SIGTERM;
    if (auto s = parsed.get_any('s', "signal")) {
        auto name = std::string{*s};
        if (name == "TERM" || name == "15") sig = SIGTERM;
        else if (name == "KILL" || name == "9") sig = SIGKILL;
        else if (name == "INT" || name == "2") sig = SIGINT;
        else if (name == "HUP" || name == "1") sig = SIGHUP;
        else {
            std::fprintf(stderr, "cfbox timeout: unknown signal '%s'\n", name.c_str());
            return 1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::fprintf(stderr, "cfbox timeout: fork failed: %s\n", std::strerror(errno));
        return 1;
    }

    if (pid == 0) {
        // Child
        std::vector<std::string> arg_storage;
        for (std::size_t i = 1; i < pos.size(); ++i) {
            arg_storage.emplace_back(pos[i]);
        }
        std::vector<char*> cmd_args;
        for (auto& s : arg_storage) cmd_args.push_back(s.data());
        cmd_args.push_back(nullptr);
        execvp(cmd_args[0], cmd_args.data());
        std::fprintf(stderr, "cfbox timeout: failed to execute '%s': %s\n",
                     cmd_args[0], std::strerror(errno));
        _exit(125);
    }

    // Parent: wait with timeout
    auto usecs = static_cast<useconds_t>(duration * 1000000);
    usleep(usecs);

    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) {
        // Still running, send signal
        kill(pid, sig);
        if (auto k = parsed.get_any('k', "kill-after")) {
            double kill_dur = std::strtod(std::string{*k}.c_str(), nullptr);
            auto kill_usecs = static_cast<useconds_t>(kill_dur * 1000000);
            usleep(kill_usecs);
            result = waitpid(pid, &status, WNOHANG);
            if (result == 0) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
            }
        } else {
            waitpid(pid, &status, 0);
        }
        return 124;
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
