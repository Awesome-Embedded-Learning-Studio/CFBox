#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "nice",
    .version = CFBOX_VERSION_STRING,
    .one_line = "run a program with modified scheduling priority",
    .usage   = "nice [-n ADJUSTMENT] COMMAND [ARGS]...",
    .options = "  -n ADJUSTMENT  add ADJUSTMENT to nice value (default 10)",
    .extra   = "",
};
} // namespace

auto nice_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true, "adjustment"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int adjustment = 10;
    if (auto n = parsed.get_any('n', "adjustment")) {
        adjustment = std::stoi(std::string{*n});
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox nice: missing command\n");
        return 1;
    }

    setpriority(PRIO_PROCESS, 0, getpriority(PRIO_PROCESS, 0) + adjustment);

    std::vector<std::string> arg_storage;
    for (auto p : pos) arg_storage.emplace_back(p);
    std::vector<char*> cmd_args;
    for (auto& s : arg_storage) cmd_args.push_back(s.data());
    cmd_args.push_back(nullptr);

    execvp(cmd_args[0], cmd_args.data());
    std::fprintf(stderr, "cfbox nice: %s: %s\n", cmd_args[0], std::strerror(errno));
    return 127;
}
