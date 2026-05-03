#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <algorithm>

#include <csignal>
#include <unistd.h>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP_PGREP = {
    .name    = "pgrep",
    .version = CFBOX_VERSION_STRING,
    .one_line = "look up processes based on name",
    .usage   = "pgrep [-flx] [-P PPID] [-u UID] PATTERN",
    .options = "  -f     match against full command line\n"
               "  -l     list PID and name\n"
               "  -x     exact match\n"
               "  -P     match by parent PID\n"
               "  -u     match by effective UID",
    .extra   = "",
};

auto get_program_name(char* argv0) -> std::string {
    std::string_view sv(argv0);
    auto slash = sv.rfind('/');
    return std::string(slash == std::string_view::npos ? sv : sv.substr(slash + 1));
}

} // anonymous namespace

auto pgrep_main(int argc, char* argv[]) -> int {
    bool is_pkill = false;
    if (argc > 0) {
        auto name = get_program_name(argv[0]);
        if (name == "pkill") is_pkill = true;
    }

    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'f', false, "full"},
        cfbox::args::OptSpec{'l', false, "list"},
        cfbox::args::OptSpec{'x', false, "exact"},
        cfbox::args::OptSpec{'P', true, "parent"},
        cfbox::args::OptSpec{'u', true, "euid"},
        cfbox::args::OptSpec{'s', true, "signal"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP_PGREP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP_PGREP); return 0; }

    bool full_match = parsed.has('f') || parsed.has_long("full");
    bool list_names = parsed.has('l') || parsed.has_long("list");
    bool exact = parsed.has('x') || parsed.has_long("exact");

    pid_t filter_ppid = -1;
    if (auto v = parsed.get('P')) filter_ppid = static_cast<pid_t>(std::stoi(std::string(*v)));

    uid_t filter_uid = static_cast<uid_t>(-1);
    if (auto v = parsed.get('u')) filter_uid = static_cast<uid_t>(std::stoi(std::string(*v)));

    int sig = SIGTERM;
    if (auto v = parsed.get('s')) {
        std::string_view sname = *v;
        if (sname.size() > 3 && sname.substr(0, 3) == "SIG") sname = sname.substr(3);
        if (sname == "HUP") sig = SIGHUP;
        else if (sname == "INT") sig = SIGINT;
        else if (sname == "KILL") sig = SIGKILL;
        else if (sname == "TERM") sig = SIGTERM;
        else if (sname == "USR1") sig = SIGUSR1;
        else if (sname == "USR2") sig = SIGUSR2;
        else sig = std::stoi(std::string(*v));
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox %s: no pattern specified\n", is_pkill ? "pkill" : "pgrep");
        return 1;
    }
    const auto& pattern = pos[0];

    auto result = cfbox::proc::read_all_processes();
    if (!result) {
        std::fprintf(stderr, "cfbox %s: %s\n", is_pkill ? "pkill" : "pgrep", result.error().msg.c_str());
        return 1;
    }

    auto matches_name = [&](const cfbox::proc::ProcessInfo& p) -> bool {
        if (full_match) {
            std::string cmdline;
            for (const auto& a : p.cmdline) {
                if (!cmdline.empty()) cmdline += ' ';
                cmdline += a;
            }
            if (cmdline.empty()) cmdline = p.comm;
            if (exact) return cmdline == pattern;
            return cmdline.find(pattern) != std::string::npos;
        }
        if (exact) return p.comm == pattern;
        return p.comm.find(pattern) != std::string::npos;
    };

    bool found = false;
    for (const auto& p : *result) {
        if (filter_ppid >= 0 && p.ppid != filter_ppid) continue;
        if (filter_uid != static_cast<uid_t>(-1) && p.uid != filter_uid) continue;
        if (!matches_name(p)) continue;

        found = true;
        if (is_pkill) {
            kill(p.pid, sig);
        } else {
            if (list_names) {
                std::printf("%d %s\n", p.pid, p.comm.c_str());
            } else {
                std::printf("%d\n", p.pid);
            }
        }
    }

    return found ? 0 : 1;
}
