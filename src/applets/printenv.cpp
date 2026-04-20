#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

extern char** environ;

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "printenv",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print all or part of environment",
    .usage   = "printenv [VARIABLE]...",
    .options = "",
    .extra   = "",
};
} // namespace

auto printenv_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        for (char** env = environ; *env != nullptr; ++env) {
            std::puts(*env);
        }
        return 0;
    }

    int rc = 0;
    for (auto var : pos) {
        const char* val = std::getenv(std::string{var}.c_str());
        if (val) {
            std::puts(val);
        } else {
            rc = 1;
        }
    }
    return rc;
}
