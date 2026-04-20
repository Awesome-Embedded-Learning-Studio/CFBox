#include <cstdio>
#include <cstdlib>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "head",
    .version = CFBOX_VERSION_STRING,
    .one_line = "output the first part of files",
    .usage   = "head [OPTIONS] [FILE]...",
    .options = "  -n N   output the first N lines (default 10)\n"
               "  -c N   output the first N bytes",
    .extra   = "",
};

auto head_lines(const std::vector<std::string>& lines, long n) -> void {
    long count = (n >= 0) ? n : static_cast<long>(lines.size()) + n;
    if (count < 0) count = 0;
    for (long i = 0; i < count && i < static_cast<long>(lines.size()); ++i) {
        std::printf("%s\n", lines[static_cast<std::size_t>(i)].c_str());
    }
}

auto head_bytes(const std::string& content, long n) -> void {
    long count = (n >= 0) ? n : static_cast<long>(content.size()) + n;
    if (count < 0) count = 0;
    if (count > 0) {
        std::fwrite(content.data(), 1, static_cast<std::size_t>(count), stdout);
    }
}

auto head_file(std::string_view path, long n_lines, long n_bytes,
               bool use_lines, bool show_header) -> int {
    bool use_stdin = (path == "-");

    auto result = use_stdin ? cfbox::io::read_all_stdin() : cfbox::io::read_all(path);
    if (!result) {
        std::fprintf(stderr, "cfbox head: %s\n", result.error().msg.c_str());
        return 1;
    }

    const auto& content = result.value();

    if (show_header) {
        std::printf("==> %s <==\n", use_stdin ? "standard input" : std::string{path}.c_str());
    }

    if (use_lines) {
        auto lines = cfbox::io::split_lines(content);
        head_lines(lines, n_lines);
    } else {
        head_bytes(content, n_bytes);
    }
    return 0;
}

} // namespace

auto head_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true},
        cfbox::args::OptSpec{'c', true},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool use_lines = true;
    long n_lines = 10;
    long n_bytes = 0;

    if (parsed.has('n')) {
        n_lines = std::strtol(std::string{parsed.get('n').value_or("10")}.c_str(), nullptr, 10);
    }
    if (parsed.has('c')) {
        n_bytes = std::strtol(std::string{parsed.get('c').value_or("0")}.c_str(), nullptr, 10);
        use_lines = false;
    }

    const auto& pos = parsed.positional();
    bool multi = pos.size() > 1;

    if (pos.empty()) {
        return head_file("-", n_lines, n_bytes, use_lines, false);
    }

    int rc = 0;
    for (std::size_t i = 0; i < pos.size(); ++i) {
        if (head_file(pos[i], n_lines, n_bytes, use_lines, multi) != 0) rc = 1;
    }
    return rc;
}
