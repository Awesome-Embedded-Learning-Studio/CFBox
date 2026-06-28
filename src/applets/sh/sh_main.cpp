#include "sh.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <termios.h>
#include <unistd.h>

#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "sh",
    .version = CFBOX_VERSION_STRING,
    .one_line = "POSIX shell command interpreter",
    .usage = "sh [-c command] [script [args...]]",
    .options = "  -c CMD   execute CMD string\n"
               "  -s       read commands from stdin",
    .extra = "",
};

auto run_exit_trap(cfbox::sh::ShellState& state) -> void {
    auto cmd = state.get_trap(0);
    if (!cmd.empty()) {
        cfbox::sh::Lexer lexer(cmd);
        cfbox::sh::Parser parser(lexer);
        auto ast = parser.parse_program();
        if (ast) cfbox::sh::execute(*ast, state);
    }
}

auto run_string(const std::string& script, cfbox::sh::ShellState& state) -> int {
    cfbox::sh::Lexer lexer(script);
    cfbox::sh::Parser parser(lexer);
    auto ast = parser.parse_program();
    int rc = ast ? cfbox::sh::execute(*ast, state) : 0;
    run_exit_trap(state);
    return rc;
}

auto run_file(const char* path, cfbox::sh::ShellState& state) -> int {
    auto* fp = std::fopen(path, "r");
    if (!fp) {
        CFBOX_ERR("sh", "%s: %s", path, std::strerror(errno));
        return 127;
    }
    std::string script;
    char buf[4096];
    while (auto n = std::fread(buf, 1, sizeof(buf), fp)) {
        script.append(buf, n);
    }
    std::fclose(fp);
    return run_string(script, state);
}

// Force the controlling tty into canonical mode so the kernel handles line
// editing (backspace, Ctrl-C). Without it a serial console echoes raw control
// bytes. Restored when the guard goes out of scope — no leaked global state.
struct CookedTermios {
    int fd;
    bool changed = false;
    struct termios saved{};
    explicit CookedTermios(int f) : fd(f) {
        if (isatty(fd) && tcgetattr(fd, &saved) == 0) {
            struct termios c = saved;
            c.c_lflag |= ICANON | ECHO | ECHOE | ECHOK | ISIG;
            c.c_iflag |= ICRNL | IXON;
            c.c_oflag |= OPOST | ONLCR;
            c.c_cc[VERASE] = 0x08; // treat Ctrl-H (BS) as erase; default VERASE is often DEL
            changed = tcsetattr(fd, TCSANOW, &c) == 0;
        }
    }
    ~CookedTermios() {
        if (changed)
            (void)tcsetattr(fd, TCSANOW, &saved);
    }
};

auto run_interactive(cfbox::sh::ShellState& state) -> int {
    CookedTermios tty(STDIN_FILENO);
    std::string line;
    int last_rc = 0;

    while (!state.should_exit) {
        std::fprintf(stderr, "$ ");
        std::fflush(stderr);

        if (!std::getline(std::cin, line)) {
            std::fputc('\n', stderr);
            break;
        }

        if (line.empty())
            continue;

        cfbox::sh::Lexer lexer(line);
        cfbox::sh::Parser parser(lexer);
        auto ast = parser.parse_program();
        if (ast) {
            last_rc = cfbox::sh::execute(*ast, state);
            state.set_last_status(last_rc);
        }
    }

    run_exit_trap(state);
    return state.should_exit ? state.exit_status : last_rc;
}

} // namespace

auto sh_main(int argc, char* argv[]) -> int {
    // Manual arg parsing (sh uses its own args convention)
    std::string command;
    bool from_stdin = false;
    int script_arg = 0;

    int first_positional = 0; // index of first arg after options

    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--help") {
            cfbox::help::print_help(HELP);
            return 0;
        }
        if (arg == "--version") {
            cfbox::help::print_version(HELP);
            return 0;
        }
        if (arg == "-c" && i + 1 < argc) {
            command = argv[++i];
            first_positional = i + 1;
            break; // remaining args after -c CMD are positional
        } else if (arg == "-s") {
            from_stdin = true;
            (void)from_stdin;
        } else if (arg[0] != '-') {
            script_arg = i;
            break;
        }
    }

    cfbox::sh::ShellState state;

    // Set up positional parameters
    if (script_arg > 0) {
        state.set_script_name(argv[script_arg]);
        std::vector<std::string> params;
        for (int i = script_arg + 1; i < argc; ++i) {
            params.emplace_back(argv[i]);
        }
        state.set_positional(std::move(params));
    } else if (!command.empty() && first_positional < argc) {
        // sh -c 'cmd' name arg1 arg2 ... → $0=name, $1=arg1, ...
        if (first_positional < argc) {
            state.set_script_name(argv[first_positional]);
        }
        std::vector<std::string> params;
        for (int i = first_positional + 1; i < argc; ++i) {
            params.emplace_back(argv[i]);
        }
        state.set_positional(std::move(params));
    } else {
        state.set_script_name(argc > 0 ? argv[0] : "sh");
    }

    // Set essential env vars. cfbox init execs sh with only HOME/TERM, so PATH
    // is usually empty here — fall back to a sane default and export it so child
    // applets (which, ...) can resolve commands too.
    if (state.get_var("PATH").empty()) {
        const char* path = std::getenv("PATH");
        if (!path || !*path) {
            path = "/bin:/sbin:/usr/bin:/usr/sbin";
            setenv("PATH", path, 1);
        }
        state.set_var("PATH", path);
    }

    if (!command.empty()) {
        return run_string(command, state);
    }

    if (script_arg > 0) {
        return run_file(argv[script_arg], state);
    }

    return run_interactive(state);
}
