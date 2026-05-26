#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <grp.h>
#include <string>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "chgrp",
    .version = CFBOX_VERSION_STRING,
    .one_line = "change group ownership",
    .usage   = "chgrp [-R] [-v] GROUP FILE...",
    .options = "  -R          operate on files and directories recursively\n"
               "  -v          output a diagnostic for every file processed",
    .extra   = "",
};

} // namespace

auto chgrp_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'R', false, "recursive"},
        cfbox::args::OptSpec{'v', false, "verbose"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool recursive = parsed.has('R');
    bool verbose = parsed.has('v');
    const auto& pos = parsed.positional();

    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox chgrp: missing operand\n");
        return 2;
    }

    std::string group_name(pos[0]);
    gid_t gid;
    auto* gr = getgrnam(group_name.c_str());
    if (gr) {
        gid = gr->gr_gid;
    } else {
        gid = static_cast<gid_t>(std::strtoul(group_name.c_str(), nullptr, 10));
    }

    auto chgrp_one = [&](const std::string& path) -> int {
        if (::chown(path.c_str(), static_cast<uid_t>(-1), gid) != 0) {
            std::fprintf(stderr, "cfbox chgrp: %s: %s\n", path.c_str(), std::strerror(errno));
            return 1;
        }
        if (verbose) std::printf("group of '%s' changed\n", path.c_str());
        return 0;
    };

    int rc = 0;
    for (size_t i = 1; i < pos.size(); i++) {
        std::string path(pos[i]);
        if (recursive && std::filesystem::is_directory(path)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
                if (ec) continue;
                if (chgrp_one(entry.path().string()) != 0) rc = 1;
            }
        }
        if (chgrp_one(path) != 0) rc = 1;
    }
    return rc;
}
