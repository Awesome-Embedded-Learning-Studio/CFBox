#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "unlink",
    .version = CFBOX_VERSION_STRING,
    .one_line = "call the unlink function to remove a file",
    .usage   = "unlink FILE",
    .options = "",
    .extra   = "",
};
} // namespace

auto unlink_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox unlink: missing operand\n");
        return 1;
    }
    if (pos.size() > 1) {
        std::fprintf(stderr, "cfbox unlink: extra operand '%.*s'\n",
                     static_cast<int>(pos[1].size()), pos[1].data());
        return 1;
    }

    if (::unlink(std::string{pos[0]}.c_str()) != 0) {
        std::fprintf(stderr, "cfbox unlink: cannot unlink '%.*s': %s\n",
                     static_cast<int>(pos[0].size()), pos[0].data(), std::strerror(errno));
        return 1;
    }
    return 0;
}
