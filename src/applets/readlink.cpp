#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "readlink",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print the value of a symbolic link or canonical file name",
    .usage   = "readlink [-f] FILE...",
    .options = "  -f     canonicalize by following every symlink\n"
               "  -n     do not output the trailing newline",
    .extra   = "",
};
} // namespace

auto readlink_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'f', false},
        cfbox::args::OptSpec{'n', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool canonicalize = parsed.has('f');
    bool no_newline = parsed.has('n');
    const auto& pos = parsed.positional();

    if (pos.empty()) {
        std::fprintf(stderr, "cfbox readlink: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        std::string path{p};
        cfbox::base::Result<std::string> result;
        if (canonicalize) {
            result = cfbox::fs::canonical(path);
        } else {
            result = cfbox::fs::read_symlink(path);
        }
        if (!result) {
            std::fprintf(stderr, "cfbox readlink: %s\n", result.error().msg.c_str());
            rc = 1;
            continue;
        }
        std::fputs(result->c_str(), stdout);
        if (!no_newline) std::putchar('\n');
    }
    return rc;
}
