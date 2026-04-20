#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "mkdir",
    .version = CFBOX_VERSION_STRING,
    .one_line = "create directories",
    .usage   = "mkdir [OPTIONS] DIRECTORY...",
    .options = "  -p     no error if existing, make parent directories as needed\n"
               "  -m MODE  set file mode (as in chmod), not a=rwx - umask",
    .extra   = "",
};

auto parse_mode(std::string_view mode_str) -> std::filesystem::perms {
    unsigned long mode = std::strtoul(std::string{mode_str}.c_str(), nullptr, 8);
    return static_cast<std::filesystem::perms>(mode);
}

} // namespace

auto mkdir_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'p', false, "parents"},
        cfbox::args::OptSpec{'m', true, "mode"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool recursive = parsed.has('p');
    auto mode_val = std::filesystem::perms::all;

    if (parsed.has('m')) {
        mode_val = parse_mode(parsed.get('m').value_or("755"));
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox mkdir: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (const auto& dir : pos) {
        if (recursive) {
            auto result = cfbox::fs::mkdir_recursive(dir, mode_val);
            if (!result) {
                std::fprintf(stderr, "cfbox mkdir: cannot create directory '%s': %s\n",
                             std::string{dir}.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        } else {
            if (cfbox::fs::exists(dir)) {
                std::fprintf(stderr, "cfbox mkdir: cannot create directory '%s': File exists\n",
                             std::string{dir}.c_str());
                rc = 1;
                continue;
            }
            auto result = cfbox::fs::mkdir_single(dir, mode_val);
            if (!result) {
                std::fprintf(stderr, "cfbox mkdir: cannot create directory '%s': %s\n",
                             std::string{dir}.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        }
    }
    return rc;
}
