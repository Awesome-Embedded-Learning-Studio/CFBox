#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "dirname",
    .version = CFBOX_VERSION_STRING,
    .one_line = "strip last component from file name",
    .usage   = "dirname PATH",
    .options = "",
    .extra   = "",
};

auto do_dirname(std::string_view path) -> std::string {
    if (path.empty()) return ".";

    // Remove trailing slashes
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }

    if (path.empty()) return "/";

    auto pos = path.rfind('/');
    if (pos == std::string_view::npos) return ".";

    // Remove trailing slashes from the parent part
    auto parent = path.substr(0, pos);
    while (parent.size() > 1 && parent.back() == '/') {
        parent.remove_suffix(1);
    }

    if (parent.empty()) return "/";
    return std::string{parent};
}

} // namespace

auto dirname_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox dirname: missing operand\n");
        return 1;
    }

    std::puts(do_dirname(pos[0]).c_str());
    return 0;
}
