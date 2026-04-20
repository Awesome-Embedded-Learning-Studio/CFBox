#include <cstdio>
#include <filesystem>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "rmdir",
    .version = CFBOX_VERSION_STRING,
    .one_line = "remove empty directories",
    .usage   = "rmdir DIRECTORY...",
    .options = "",
    .extra   = "",
};
} // namespace

auto rmdir_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox rmdir: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path{p}, ec);
        if (ec) {
            std::fprintf(stderr, "cfbox rmdir: failed to remove '%.*s': %s\n",
                         static_cast<int>(p.size()), p.data(), ec.message().c_str());
            rc = 1;
        }
    }
    return rc;
}
