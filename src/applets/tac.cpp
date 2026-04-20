#include <cstdio>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "tac",
    .version = CFBOX_VERSION_STRING,
    .one_line = "concatenate and print files in reverse",
    .usage   = "tac [FILE]...",
    .options = "",
    .extra   = "",
};
} // namespace

auto tac_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        std::vector<std::string> lines;
        auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
            lines.push_back(line);
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox tac: %s\n", result.error().msg.c_str());
            rc = 1;
            continue;
        }
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            std::puts(it->c_str());
        }
    }
    return rc;
}
