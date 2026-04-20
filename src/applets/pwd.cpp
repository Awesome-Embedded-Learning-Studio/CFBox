#include <cstdio>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "pwd",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print working directory",
    .usage   = "pwd [-L|-P]",
    .options = "  -L     logical path (default)\n"
               "  -P     physical path (no symlinks)",
    .extra   = "",
};
} // namespace

auto pwd_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'L', false},
        cfbox::args::OptSpec{'P', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto result = cfbox::fs::current_path();
    if (!result) {
        std::fprintf(stderr, "cfbox pwd: %s\n", result.error().msg.c_str());
        return 1;
    }
    std::puts(result->c_str());
    return 0;
}
