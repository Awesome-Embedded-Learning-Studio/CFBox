#include <cstdio>
#include <cstdlib>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "truncate",
    .version = CFBOX_VERSION_STRING,
    .one_line = "shrink or extend the size of a file to the specified size",
    .usage   = "truncate -s SIZE FILE...",
    .options = "  -s SIZE  set file size (supports K, M, G suffixes)",
    .extra   = "",
};

auto parse_size(const std::string& s) -> long {
    char* end = nullptr;
    long val = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str()) return -1;
    if (*end != '\0') {
        switch (*end) {
            case 'K': case 'k': val *= 1024; break;
            case 'M': case 'm': val *= 1024 * 1024; break;
            case 'G': case 'g': val *= 1024L * 1024 * 1024; break;
            default: return -1;
        }
    }
    return val;
}
} // namespace

auto truncate_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', true, "size"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto size_str = parsed.get_any('s', "size");
    if (!size_str) {
        std::fprintf(stderr, "cfbox truncate: missing operand: -s SIZE\n");
        return 1;
    }

    long size = parse_size(std::string{*size_str});
    if (size < 0) {
        std::fprintf(stderr, "cfbox truncate: invalid size: '%.*s'\n",
                     static_cast<int>(size_str->size()), size_str->data());
        return 1;
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox truncate: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        auto result = cfbox::fs::resize_file(std::string{p}, static_cast<std::uintmax_t>(size));
        if (!result) {
            std::fprintf(stderr, "cfbox truncate: %s\n", result.error().msg.c_str());
            rc = 1;
        }
    }
    return rc;
}
