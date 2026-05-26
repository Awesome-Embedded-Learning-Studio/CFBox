#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "rm",
    .version = CFBOX_VERSION_STRING,
    .one_line = "remove files or directories",
    .usage   = "rm [OPTIONS] FILE...",
    .options = "  -r     remove directories and their contents recursively\n"
               "  -f     ignore nonexistent files, never prompt",
    .extra   = "",
};

auto is_root_path(std::string_view path) -> bool {
    // Remove trailing slashes for comparison
    std::string_view p = path;
    while (p.size() > 1 && p.back() == '/') {
        p.remove_suffix(1);
    }
    return p == "/";
}

} // namespace

auto rm_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'r', false, "recursive"},
        cfbox::args::OptSpec{'f', false, "force"},
        cfbox::args::OptSpec{'i', false, "interactive"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool recursive = parsed.has('r');
    bool force = parsed.has('f');
    // -i: interactive (MVP: auto-confirm)

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        if (!force) {
            CFBOX_ERR("rm", "missing operand");
            return 1;
        }
        return 0;
    }

    int rc = 0;
    for (const auto& target : pos) {
        // Safety check: prevent removing /
        if (is_root_path(target)) {
            CFBOX_ERR("rm", "refusing to remove '%s'", std::string{target}.c_str());
            rc = 1;
            continue;
        }

        if (!cfbox::fs::exists(target)) {
            if (!force) {
                CFBOX_ERR("rm", "cannot remove '%s': No such file or directory", std::string{target}.c_str());
                rc = 1;
            }
            continue;
        }

        if (cfbox::fs::is_directory(target)) {
            if (!recursive) {
                CFBOX_ERR("rm", "cannot remove '%s': Is a directory", std::string{target}.c_str());
                rc = 1;
                continue;
            }
            auto result = cfbox::fs::remove_all(target);
            if (!result) {
                CFBOX_ERR("rm", "cannot remove '%s': %s", std::string{target}.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        } else {
            auto result = cfbox::fs::remove_single(target);
            if (!result) {
                CFBOX_ERR("rm", "cannot remove '%s': %s", std::string{target}.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        }
    }
    return rc;
}
