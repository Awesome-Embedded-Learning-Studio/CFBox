#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "tee",
    .version = CFBOX_VERSION_STRING,
    .one_line = "read from stdin and write to stdout and files",
    .usage   = "tee [-a] [FILE]...",
    .options = "  -a     append to the given FILEs, do not overwrite",
    .extra   = "",
};
} // namespace

auto tee_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'a', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool append = parsed.has('a');
    const auto& pos = parsed.positional();

    std::vector<std::FILE*> files;
    for (auto p : pos) {
        auto* f = std::fopen(std::string{p}.c_str(), append ? "ab" : "wb");
        if (!f) {
            std::fprintf(stderr, "cfbox tee: %s: %s\n", std::string{p}.c_str(), std::strerror(errno));
        } else {
            files.push_back(f);
        }
    }

    char buf[4096];
    int rc = 0;
    while (auto n = std::fread(buf, 1, sizeof(buf), stdin)) {
        std::fwrite(buf, 1, n, stdout);
        for (auto* f : files) {
            if (std::fwrite(buf, 1, n, f) != n) {
                rc = 1;
            }
        }
    }

    for (auto* f : files) {
        std::fclose(f);
    }
    return rc;
}
