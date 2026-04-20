#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <signal.h>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "nohup",
    .version = CFBOX_VERSION_STRING,
    .one_line = "run a command immune to hangups",
    .usage   = "nohup COMMAND [ARGS]...",
    .options = "",
    .extra   = "Output is appended to nohup.out or $HOME/nohup.out.",
};
} // namespace

auto nohup_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox nohup: missing command\n");
        return 1;
    }

    signal(SIGHUP, SIG_IGN);

    // Redirect stdout/stderr to nohup.out
    std::string outfile = "nohup.out";
    if (const char* home = std::getenv("HOME")) {
        // Only use $HOME/nohup.out if cwd is not writable
        if (access(".", W_OK) != 0) {
            outfile = std::string{home} + "/nohup.out";
        }
    }

    auto* f = freopen(outfile.c_str(), "a", stdout);
    if (!f) {
        std::fprintf(stderr, "cfbox nohup: cannot open %s: %s\n",
                     outfile.c_str(), std::strerror(errno));
        return 1;
    }
    dup2(fileno(stdout), STDERR_FILENO);

    std::vector<std::string> arg_storage;
    for (auto p : pos) arg_storage.emplace_back(p);
    std::vector<char*> cmd_args;
    for (auto& s : arg_storage) cmd_args.push_back(s.data());
    cmd_args.push_back(nullptr);

    execvp(cmd_args[0], cmd_args.data());
    std::fprintf(stderr, "cfbox nohup: %s: %s\n", cmd_args[0], std::strerror(errno));
    return 127;
}
