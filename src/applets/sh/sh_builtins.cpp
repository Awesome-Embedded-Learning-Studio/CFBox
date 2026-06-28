#include "sh.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <cfbox/error.hpp>
#include <cfbox/io.hpp>

namespace cfbox::sh {

static int builtin_echo(std::vector<std::string>& args, ShellState& /*state*/) {
    bool no_newline = false;
    std::size_t start = 1;

    if (args.size() > 1 && args[1] == "-n") {
        no_newline = true;
        start = 2;
    }

    for (std::size_t i = start; i < args.size(); ++i) {
        if (i > start) std::fputc(' ', stdout);
        std::fputs(args[i].c_str(), stdout);
    }
    if (!no_newline) std::fputc('\n', stdout);
    return 0;
}

static int builtin_cd(std::vector<std::string>& args, ShellState& state) {
    std::string dir;
    if (args.size() > 1) {
        dir = args[1];
        if (dir == "-") {
            dir = state.get_var("OLDPWD");
            if (dir.empty()) dir = state.get_var("HOME");
        }
    } else {
        dir = state.get_var("HOME");
        if (dir.empty()) dir = "/";
    }

    std::string old = std::filesystem::current_path().string();
    if (::chdir(dir.c_str()) != 0) {
        CFBOX_ERR("sh", "cd: %s: %s", dir.c_str(), std::strerror(errno));
        return 1;
    }
    state.set_var("OLDPWD", old);
    state.set_var("PWD", std::filesystem::current_path().string());
    return 0;
}

static int builtin_pwd(std::vector<std::string>& /*args*/, ShellState& /*state*/) {
    std::puts(std::filesystem::current_path().string().c_str());
    return 0;
}

static int builtin_export(std::vector<std::string>& args, ShellState& state) {
    if (args.size() == 1) {
        // Print all exported vars
        for (auto& [name, value] : state.all_vars()) {
            if (state.is_exported(name)) {
                std::printf("export %s=\"%s\"\n", name.c_str(), value.c_str());
            }
        }
        return 0;
    }

    for (std::size_t i = 1; i < args.size(); ++i) {
        auto& arg = args[i];
        auto eq = arg.find('=');
        if (eq != std::string::npos) {
            auto name = arg.substr(0, eq);
            auto value = arg.substr(eq + 1);
            state.set_var(name, value);
            state.export_var(name);
        } else {
            state.export_var(arg);
        }
    }
    return 0;
}

static int builtin_exit(std::vector<std::string>& args, ShellState& state) {
    int code = state.last_status();
    if (args.size() > 1) {
        code = std::atoi(args[1].c_str());
    }
    state.should_exit = true;
    state.exit_status = code;
    return code;
}

static int builtin_true(std::vector<std::string>& /*args*/, ShellState& /*state*/) {
    return 0;
}

static int builtin_false(std::vector<std::string>& /*args*/, ShellState& /*state*/) {
    return 1;
}

static int builtin_colon(std::vector<std::string>& /*args*/, ShellState& /*state*/) {
    return 0;
}

static int builtin_set(std::vector<std::string>& args, ShellState& state) {
    if (args.size() > 1) {
        // set -- arg1 arg2 ... → replace positional params
        if (args[1] == "--") {
            std::vector<std::string> params;
            for (std::size_t i = 2; i < args.size(); ++i) {
                params.push_back(args[i]);
            }
            state.set_positional(std::move(params));
            return 0;
        }
    }
    // Print all vars
    for (auto& [name, value] : state.all_vars()) {
        std::printf("%s='%s'\n", name.c_str(), value.c_str());
    }
    return 0;
}

static int builtin_unset(std::vector<std::string>& args, ShellState& state) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        state.unset_var(args[i]);
    }
    return 0;
}

static int builtin_shift(std::vector<std::string>& args, ShellState& state) {
    int n = 1;
    if (args.size() > 1) n = std::atoi(args[1].c_str());
    if (n < 0) n = 0;
    if (n > static_cast<int>(state.positional_params().size())) return 1;
    state.shift(n);
    return 0;
}

static int builtin_read(std::vector<std::string>& args, ShellState& state) {
    std::string prompt;
    // Options: -r (raw, keep backslashes — we already do), -p PROMPT.
    std::size_t i = 1;
    while (i < args.size() && args[i].size() >= 2 && args[i][0] == '-' && args[i] != "--") {
        if (args[i] == "-r") {
            ++i;
        } else if (args[i] == "-p") {
            ++i;
            if (i < args.size()) { prompt = args[i]; ++i; }
        } else if (args[i].size() > 2 && args[i][1] == 'p') {
            prompt = args[i].substr(2);
            ++i;
        } else {
            break;
        }
    }
    if (i < args.size() && args[i] == "--") ++i;

    if (!prompt.empty()) std::fputs(prompt.c_str(), stderr);

    std::string line;
    if (!std::getline(std::cin, line)) return 1;

    std::vector<std::string> names(args.begin() + static_cast<long>(i), args.end());
    if (names.empty()) {
        state.set_var("REPLY", line);
        return 0;
    }

    // Split line by IFS into the named variables; the last gets the remainder.
    std::string ifs = state.get_var("IFS");
    if (ifs.empty()) ifs = " \t\n";

    std::size_t pos = 0;
    for (std::size_t k = 0; k < names.size(); ++k) {
        if (k == names.size() - 1) {
            while (pos < line.size() && ifs.find(line[pos]) != std::string::npos) ++pos;
            state.set_var(names[k], line.substr(pos));
            break;
        }
        while (pos < line.size() && ifs.find(line[pos]) != std::string::npos) ++pos;
        std::size_t end = pos;
        while (end < line.size() && ifs.find(line[end]) == std::string::npos) ++end;
        state.set_var(names[k], line.substr(pos, end - pos));
        pos = end;
    }
    return 0;
}

static int builtin_eval(std::vector<std::string>& args, ShellState& state) {
    if (args.size() <= 1) return 0;

    std::string cmd;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (i > 1) cmd += ' ';
        cmd += args[i];
    }

    Lexer lexer(cmd);
    Parser parser(lexer);
    auto ast = parser.parse_program();
    if (ast) return execute(*ast, state);
    return 0;
}

static int builtin_source(std::vector<std::string>& args, ShellState& state) {
    if (args.size() <= 1) {
        CFBOX_ERR("sh", "source: missing argument");
        return 2;
    }

    auto script_result = cfbox::io::read_all(args[1]);
    if (!script_result) {
        CFBOX_ERR("sh", "%s: %s", args[1].c_str(),
                  std::strerror(script_result.error().code));
        return 1;
    }

    Lexer lexer(*script_result);
    Parser parser(lexer);
    auto ast = parser.parse_program();
    if (ast) return execute(*ast, state);
    return 0;
}

static int builtin_return(std::vector<std::string>& args, ShellState& state) {
    int code = state.last_status();
    if (args.size() > 1) code = std::atoi(args[1].c_str());
    state.return_pending = true;
    state.return_status = code;
    return code;
}

static int builtin_local(std::vector<std::string>& args, ShellState& state) {
    if (!state.in_function()) {
        CFBOX_ERR("sh", "local: can only be used in a function");
        return 1;
    }
    for (std::size_t i = 1; i < args.size(); ++i) {
        auto& arg = args[i];
        auto eq = arg.find('=');
        std::string name = (eq == std::string::npos) ? arg : arg.substr(0, eq);
        std::string val = (eq == std::string::npos) ? state.get_var(name) : arg.substr(eq + 1);
        state.set_local(name, val);
    }
    return 0;
}

static int builtin_break(std::vector<std::string>& args, ShellState& state) {
    int n = (args.size() > 1) ? std::atoi(args[1].c_str()) : 1;
    if (n < 1) n = 1;
    state.break_depth = n;
    return 0;
}

static int builtin_continue(std::vector<std::string>& /*args*/, ShellState& state) {
    state.continue_loop = true;
    return 0;
}

// Installed for any signal with a trap; just records which signal fired. The
// executor runs the corresponding trap command at the next safe point.
static void trap_handler(int sig) {
    cfbox::sh::trap_pending_signal = sig;
}

static auto signal_from_name(const std::string& name) -> int {
    if (name == "EXIT" || name == "0") return 0;
    if (name == "INT" || name == "SIGINT") return SIGINT;
    if (name == "TERM" || name == "SIGTERM") return SIGTERM;
    if (name == "HUP" || name == "SIGHUP") return SIGHUP;
    if (name == "QUIT" || name == "SIGQUIT") return SIGQUIT;
    char* end = nullptr;
    long n = std::strtol(name.c_str(), &end, 10);
    if (*end == '\0' && n >= 0) return static_cast<int>(n);
    return -1;
}

static int builtin_trap(std::vector<std::string>& args, ShellState& state) {
    if (args.size() == 1) {
        for (const auto& [sig, cmd] : state.all_traps()) {
            std::printf("trap -- '%s' %d\n", cmd.c_str(), sig);
        }
        return 0;
    }

    std::size_t i = 1;
    std::string cmd;
    bool clear = false;
    if (args[i] == "-") { clear = true; ++i; }
    else if (args[i] == "-l") { return 0; }  // list signal names: not implemented
    else { cmd = args[i]; ++i; }

    for (; i < args.size(); ++i) {
        int sig = signal_from_name(args[i]);
        if (sig < 0) {
            CFBOX_ERR("sh", "trap: %s: bad signal", args[i].c_str());
            return 1;
        }
        if (clear || cmd.empty()) {
            state.clear_trap(sig);
            if (sig != 0) ::signal(sig, SIG_DFL);
        } else {
            state.set_trap(sig, cmd);
            if (sig != 0) ::signal(sig, trap_handler);
        }
    }
    return 0;
}

auto get_builtins() -> const std::unordered_map<std::string, BuiltinFunc>& {
    static const std::unordered_map<std::string, BuiltinFunc> builtins = {
        {"echo", builtin_echo},
        {"cd", builtin_cd},
        {"pwd", builtin_pwd},
        {"export", builtin_export},
        {"exit", builtin_exit},
        {"true", builtin_true},
        {"false", builtin_false},
        {":", builtin_colon},
        {"set", builtin_set},
        {"unset", builtin_unset},
        {"shift", builtin_shift},
        {"read", builtin_read},
        {"eval", builtin_eval},
        {"source", builtin_source},
        {".", builtin_source},
        {"return", builtin_return},
        {"local", builtin_local},
        {"break", builtin_break},
        {"continue", builtin_continue},
        {"trap", builtin_trap},
    };
    return builtins;
}

auto is_builtin(const std::string& name) -> bool {
    return get_builtins().count(name) > 0;
}

auto run_builtin(const std::string& name, std::vector<std::string>& args, ShellState& state) -> int {
    auto& builtins = get_builtins();
    auto it = builtins.find(name);
    if (it == builtins.end()) return 127;
    return it->second(args, state);
}

} // namespace cfbox::sh
