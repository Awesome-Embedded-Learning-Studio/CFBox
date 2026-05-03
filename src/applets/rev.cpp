#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "rev",
    .version = CFBOX_VERSION_STRING,
    .one_line = "reverse lines characterwise",
    .usage   = "rev [FILE...]",
    .options = "",
    .extra   = "",
};

auto process_stream(std::istream& in) -> void {
    std::string line;
    while (std::getline(in, line)) {
        std::reverse(line.begin(), line.end());
        std::printf("%s\n", line.c_str());
    }
}

} // anonymous namespace

auto rev_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        process_stream(std::cin);
        return 0;
    }

    int rc = 0;
    for (const auto& filename : pos) {
        auto fn = std::string(filename);
        if (fn == "-") {
            process_stream(std::cin);
        } else {
            std::ifstream f(fn);
            if (!f) {
                std::fprintf(stderr, "cfbox rev: cannot open %s\n", fn.c_str());
                rc = 1;
                continue;
            }
            process_stream(f);
        }
    }
    return rc;
}
