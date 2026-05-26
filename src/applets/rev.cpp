#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "rev",
    .version = CFBOX_VERSION_STRING,
    .one_line = "reverse lines characterwise",
    .usage   = "rev [FILE...]",
    .options = "",
    .extra   = "",
};

auto process_stream(std::FILE* f) -> void {
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) {
        auto len = std::strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        std::reverse(buf, buf + len);
        std::printf("%s\n", buf);
    }
}

} // anonymous namespace

auto rev_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        process_stream(stdin);
        return 0;
    }

    int rc = 0;
    for (const auto& filename : pos) {
        auto fn = std::string(filename);
        if (fn == "-") {
            process_stream(stdin);
        } else {
            auto* f = std::fopen(fn.c_str(), "r");
            if (!f) {
                CFBOX_ERR("rev", "cannot open %s", fn.c_str());
                rc = 1;
                continue;
            }
            process_stream(f);
            std::fclose(f);
        }
    }
    return rc;
}
