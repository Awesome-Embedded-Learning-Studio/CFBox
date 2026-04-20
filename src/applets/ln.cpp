#include <cstdio>
#include <cstring>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "ln",
    .version = CFBOX_VERSION_STRING,
    .one_line = "make links between files",
    .usage   = "ln [-s] [-f] [-n] TARGET... DIRECTORY",
    .options = "  -s     make symbolic links instead of hard links\n"
               "  -f     remove existing destination files\n"
               "  -n     treat LINK_NAME as a normal file if it is a symlink to a dir",
    .extra   = "",
};
} // namespace

auto ln_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false},
        cfbox::args::OptSpec{'f', false},
        cfbox::args::OptSpec{'n', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool symbolic = parsed.has('s');
    bool force = parsed.has('f');
    const auto& pos = parsed.positional();

    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox ln: missing operand\n");
        return 1;
    }

    std::string target{pos[0]};
    std::string link_name{pos[1]};

    // If link_name is an existing directory, put link inside it
    if (pos.size() == 2 && cfbox::fs::is_directory(link_name)) {
        auto filename = std::filesystem::path{target}.filename();
        link_name = (std::filesystem::path{link_name} / filename).string();
    }

    if (force) {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path{link_name}, ec);
    }

    cfbox::base::Result<void> result;
    if (symbolic) {
        result = cfbox::fs::create_symlink(target, link_name);
    } else {
        result = cfbox::fs::create_hard_link(target, link_name);
    }
    if (!result) {
        std::fprintf(stderr, "cfbox ln: %s\n", result.error().msg.c_str());
        return 1;
    }
    return 0;
}
