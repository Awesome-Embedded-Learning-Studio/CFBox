#include "sh.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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
        std::fprintf(stderr, "cfbox sh: cd: %s: %s\n", dir.c_str(), std::strerror(errno));
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
    std::string line;
    if (!std::getline(std::cin, line)) return 1;

    if (args.size() <= 1) {
        state.set_var("REPLY", line);
        return 0;
    }

    // Split line by IFS into variables
    std::string ifs = state.get_var("IFS");
    if (ifs.empty()) ifs = " \t\n";

    std::size_t pos = 0;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (i == args.size() - 1) {
            // Last variable gets the rest
            state.set_var(args[i], line.substr(pos));
            break;
        }

        // Skip leading IFS
        while (pos < line.size() && ifs.find(line[pos]) != std::string::npos) ++pos;
        std::size_t end = pos;
        while (end < line.size() && ifs.find(line[end]) == std::string::npos) ++end;

        state.set_var(args[i], line.substr(pos, end - pos));
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
        std::fprintf(stderr, "cfbox sh: source: missing argument\n");
        return 2;
    }

    auto* fp = std::fopen(args[1].c_str(), "r");
    if (!fp) {
        std::fprintf(stderr, "cfbox sh: %s: %s\n", args[1].c_str(), std::strerror(errno));
        return 1;
    }

    std::string script;
    char buf[4096];
    while (auto n = std::fread(buf, 1, sizeof(buf), fp)) {
        script.append(buf, n);
    }
    std::fclose(fp);

    Lexer lexer(script);
    Parser parser(lexer);
    auto ast = parser.parse_program();
    if (ast) return execute(*ast, state);
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
