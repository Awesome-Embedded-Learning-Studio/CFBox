#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

extern char** environ;

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "env",
    .version = CFBOX_VERSION_STRING,
    .one_line = "run a program in a modified environment",
    .usage   = "env [-i] [NAME=VALUE]... [COMMAND [ARGS]...]",
    .options = "  -i     start with an empty environment",
    .extra   = "",
};
} // namespace

auto env_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'i', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool clear_env = parsed.has('i');

    // Collect NAME=VALUE assignments from positional args until we hit a non-assignment
    std::vector<std::pair<std::string, std::string>> assignments;
    const auto& pos = parsed.positional();
    std::size_t cmd_start = pos.size();

    for (std::size_t i = 0; i < pos.size(); ++i) {
        auto arg = std::string{pos[i]};
        auto eq = arg.find('=');
        if (eq == std::string::npos || eq == 0) {
            cmd_start = i;
            break;
        }
        assignments.emplace_back(arg.substr(0, eq), arg.substr(eq + 1));
    }

    if (clear_env) {
        // Clear environment, then set assigned vars
        for (char** env = environ; *env != nullptr; ++env) {
            auto eq = std::string_view{*env}.find('=');
            if (eq != std::string_view::npos) {
                std::string name{*env, eq};
                unsetenv(name.c_str());
            }
        }
    }

    for (const auto& [name, value] : assignments) {
        setenv(name.c_str(), value.c_str(), 1);
    }

    // No command: print environment
    if (cmd_start >= pos.size()) {
        for (char** env = environ; *env != nullptr; ++env) {
            std::puts(*env);
        }
        return 0;
    }

    // Execute command
    std::string cmd{pos[cmd_start]};
    std::vector<std::string> arg_storage;
    for (std::size_t i = cmd_start; i < pos.size(); ++i) {
        arg_storage.emplace_back(pos[i]);
    }
    std::vector<char*> cmd_args;
    for (auto& s : arg_storage) {
        cmd_args.push_back(s.data());
    }
    cmd_args.push_back(nullptr);

    execvp(cmd.c_str(), cmd_args.data());
    std::fprintf(stderr, "cfbox env: failed to execute '%s': %s\n",
                 cmd.c_str(), std::strerror(errno));
    return 127;
}
