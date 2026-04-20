#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "realpath",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print the resolved path",
    .usage   = "realpath FILE...",
    .options = "",
    .extra   = "",
};
} // namespace

auto realpath_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox realpath: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        auto result = cfbox::fs::canonical(std::string{p});
        if (!result) {
            std::fprintf(stderr, "cfbox realpath: %s\n", result.error().msg.c_str());
            rc = 1;
            continue;
        }
        std::puts(result->c_str());
    }
    return rc;
}
