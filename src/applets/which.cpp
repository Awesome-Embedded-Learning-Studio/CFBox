#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "which",
    .version = CFBOX_VERSION_STRING,
    .one_line = "locate a command",
    .usage   = "which [-a] COMMAND...",
    .options = "  -a     print all matching pathnames",
    .extra   = "",
};

auto find_in_path(std::string_view cmd, bool all) -> int {
    const char* path_env = std::getenv("PATH");
    if (!path_env || path_env[0] == '\0') return 1;

    bool found = false;
    std::string_view path = path_env;
    size_t start = 0;
    while (start < path.size()) {
        auto sep = path.find(':', start);
        auto dir = path.substr(start, sep == std::string_view::npos ? path.size() - start : sep - start);
        start = sep == std::string_view::npos ? path.size() : sep + 1;

        if (dir.empty()) continue;
        std::string full(dir);
        full += '/';
        full += cmd;
        if (access(full.c_str(), X_OK) == 0) {
            std::puts(full.c_str());
            found = true;
            if (!all) return 0;
        }
    }
    return found ? 0 : 1;
}

} // namespace

auto which_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'a', false, "all"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool all = parsed.has('a');
    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox which: missing argument\n");
        return 2;
    }

    int rc = 0;
    for (const auto& cmd : pos) {
        if (find_in_path(cmd, all) != 0) {
            std::fprintf(stderr, "cfbox which: no %.*s in PATH\n",
                         static_cast<int>(cmd.size()), cmd.data());
            rc = 1;
        }
    }
    return rc;
}
