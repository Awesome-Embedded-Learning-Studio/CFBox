#include <cstdio>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "link",
    .version = CFBOX_VERSION_STRING,
    .one_line = "create a hard link",
    .usage   = "link FILE1 FILE2",
    .options = "",
    .extra   = "",
};
} // namespace

auto link_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        CFBOX_ERR("link", "missing operand");
        return 1;
    }

    auto result = cfbox::fs::create_hard_link(pos[0], pos[1]);
    if (!result) {
        CFBOX_ERR("link", "%s", result.error().msg.c_str());
        return 1;
    }
    return 0;
}
