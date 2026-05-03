#include <cstdio>
#include <cstring>
#include <csignal>
#include <string>
#include <string_view>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "kill",
    .version = CFBOX_VERSION_STRING,
    .one_line = "send a signal to a process",
    .usage   = "kill [-s SIGNAL] PID...\nkill -l [SIGNAL]",
    .options = "  -s SIG   signal to send (default: TERM)\n"
               "  -l       list signal names",
    .extra   = "",
};

struct SignalEntry {
    const char* name;
    int num;
};

const SignalEntry SIGNALS[] = {
    {"HUP", SIGHUP},   {"INT", SIGINT},   {"QUIT", SIGQUIT}, {"ILL", SIGILL},
    {"ABRT", SIGABRT}, {"FPE", SIGFPE},   {"KILL", SIGKILL}, {"SEGV", SIGSEGV},
    {"PIPE", SIGPIPE}, {"ALRM", SIGALRM}, {"TERM", SIGTERM}, {"USR1", SIGUSR1},
    {"USR2", SIGUSR2}, {"CHLD", SIGCHLD}, {"CONT", SIGCONT}, {"STOP", SIGSTOP},
    {"TSTP", SIGTSTP}, {"TTIN", SIGTTIN}, {"TTOU", SIGTTOU}, {"BUS", SIGBUS},
    {"PROF", SIGPROF}, {"SYS", SIGSYS},   {"TRAP", SIGTRAP}, {"URG", SIGURG},
    {"VTALRM", SIGVTALRM}, {"XCPU", SIGXCPU}, {"XFSZ", SIGXFSZ},
};

auto signal_from_name(std::string_view name) -> int {
    // Strip optional SIG prefix
    if (name.size() > 3 && name.substr(0, 3) == "SIG")
        name = name.substr(3);

    // Try numeric
    if (!name.empty() && name[0] >= '0' && name[0] <= '9') {
        return std::atoi(name.data());
    }

    for (const auto& s : SIGNALS) {
        if (name == s.name) return s.num;
    }
    return -1;
}

auto list_signals() -> void {
    bool first = true;
    for (const auto& s : SIGNALS) {
        if (!first) std::printf(" ");
        std::printf("%2d) %-6s", s.num, s.name);
        first = false;
    }
    std::printf("\n");
}

} // anonymous namespace

auto kill_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', true, "signal"},
        cfbox::args::OptSpec{'l', false, "list"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    if (parsed.has('l') || parsed.has_long("list")) {
        list_signals();
        return 0;
    }

    int sig = SIGTERM;

    // Handle -s SIGNAL or -SIGNAL (e.g. -9, -SIGTERM)
    if (auto s = parsed.get('s')) {
        sig = signal_from_name(*s);
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox kill: no PID specified\n");
        return 1;
    }

    // If first positional looks like a signal spec (negative number or starts with SIG)
    // and no -s was given, treat it as signal
    std::size_t start = 0;
    if (!parsed.get('s') && !pos.empty()) {
        if (pos[0][0] == '-') {
            // -N or -SIGNAME
            std::string_view spec(pos[0].substr(1));
            int maybe_sig = signal_from_name(spec);
            if (maybe_sig > 0) {
                sig = maybe_sig;
                start = 1;
            }
        }
    }

    if (sig <= 0) {
        std::fprintf(stderr, "cfbox kill: invalid signal\n");
        return 1;
    }

    int errors = 0;
    for (std::size_t i = start; i < pos.size(); ++i) {
        const auto& arg = pos[i];
        // Handle special case: 0 means all processes in process group
        pid_t pid = static_cast<pid_t>(std::strtol(arg.data(), nullptr, 10));
        if (kill(pid, sig) != 0) {
            std::fprintf(stderr, "cfbox kill: (%d) - %s\n", pid, std::strerror(errno));
            ++errors;
        }
    }

    return errors > 0 ? 1 : 0;
}
