#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "basename",
    .version = CFBOX_VERSION_STRING,
    .one_line = "strip directory and suffix from file names",
    .usage   = "basename PATH [SUFFIX]",
    .options = "",
    .extra   = "",
};

auto do_basename(std::string_view path, std::string_view suffix) -> std::string {
    // Remove trailing slashes (but keep root "/")
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }

    std::string result;
    if (path.empty()) {
        result = ".";
    } else if (path == "/") {
        result = "/";
    } else {
        // Find last component after last '/'
        auto pos = path.rfind('/');
        if (pos == std::string_view::npos) {
            result = std::string{path};
        } else {
            result = std::string{path.substr(pos + 1)};
        }
    }

    // Strip suffix if given and result ends with it (but not if suffix == result)
    if (!suffix.empty() && result.size() > suffix.size()) {
        if (result.compare(result.size() - suffix.size(), suffix.size(), suffix) == 0) {
            result.erase(result.size() - suffix.size());
        }
    }

    return result;
}

} // namespace

auto basename_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox basename: missing operand\n");
        return 1;
    }

    std::string_view suffix;
    if (pos.size() >= 2) {
        suffix = pos[1];
    }

    std::puts(do_basename(pos[0], suffix).c_str());
    return 0;
}
