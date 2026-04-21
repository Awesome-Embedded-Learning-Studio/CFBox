#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/compress.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "gunzip",
    .version = CFBOX_VERSION_STRING,
    .one_line = "decompress files",
    .usage   = "gunzip [FILE]...",
    .options = "",
    .extra   = "",
};
} // namespace

auto gunzip_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();

    if (pos.empty()) {
        auto input = cfbox::io::read_all_stdin();
        if (!input) { std::fprintf(stderr, "cfbox gunzip: read error\n"); return 1; }
        auto result = cfbox::compress::gzip_decompress(*input);
        std::fwrite(result.data(), 1, result.size(), stdout);
        return 0;
    }

    int rc = 0;
    for (auto p : pos) {
        std::string path{p};
        auto input = cfbox::io::read_all(path);
        if (!input) {
            std::fprintf(stderr, "cfbox gunzip: %s\n", input.error().msg.c_str());
            rc = 1;
            continue;
        }
        auto result = cfbox::compress::gzip_decompress(*input);
        std::string outpath = (path.size() > 3 && path.substr(path.size() - 3) == ".gz")
            ? path.substr(0, path.size() - 3) : path + ".out";
        auto wresult = cfbox::io::write_all(outpath, result);
        if (!wresult) {
            std::fprintf(stderr, "cfbox gunzip: %s\n", wresult.error().msg.c_str());
            rc = 1;
        }
    }
    return rc;
}
