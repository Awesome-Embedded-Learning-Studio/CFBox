#include <cstdio>
#include <cstdlib>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/io.hpp>

namespace {

auto tail_lines(const std::vector<std::string>& lines, long n, bool from_start) -> void {
    if (from_start) {
        long start = n - 1;
        if (start < 0) start = 0;
        for (long i = start; i < static_cast<long>(lines.size()); ++i) {
            std::printf("%s\n", lines[static_cast<std::size_t>(i)].c_str());
        }
    } else {
        if (n <= 0) return;
        long start = static_cast<long>(lines.size()) - n;
        if (start < 0) start = 0;
        for (long i = start; i < static_cast<long>(lines.size()); ++i) {
            std::printf("%s\n", lines[static_cast<std::size_t>(i)].c_str());
        }
    }
}

auto tail_bytes(const std::string& content, long n) -> void {
    if (n <= 0) return;
    long start = static_cast<long>(content.size()) - n;
    if (start < 0) start = 0;
    std::fwrite(content.data() + start, 1, static_cast<std::size_t>(n), stdout);
}

auto tail_file(std::string_view path, long n, bool use_bytes,
               bool from_start, bool show_header) -> int {
    bool use_stdin = (path == "-");

    auto result = use_stdin ? cfbox::io::read_all_stdin() : cfbox::io::read_all(path);
    if (!result) {
        std::fprintf(stderr, "cfbox tail: %s\n", result.error().msg.c_str());
        return 1;
    }

    const auto& content = result.value();

    if (show_header) {
        std::printf("==> %s <==\n", use_stdin ? "standard input" : std::string{path}.c_str());
    }

    if (use_bytes) {
        tail_bytes(content, n);
    } else {
        auto lines = cfbox::io::split_lines(content);
        tail_lines(lines, n, from_start);
    }
    return 0;
}

auto parse_count(std::string_view val, bool& from_start) -> long {
    if (!val.empty() && val[0] == '+') {
        from_start = true;
        return std::strtol(std::string{val.substr(1)}.c_str(), nullptr, 10);
    }
    return std::strtol(std::string{val}.c_str(), nullptr, 10);
}

} // namespace

auto tail_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true},
        cfbox::args::OptSpec{'c', true},
    });

    bool use_lines = true;
    bool use_bytes = false;
    bool from_start = false;
    long n = 10;

    if (parsed.has('n')) {
        n = parse_count(parsed.get('n').value_or("10"), from_start);
    }
    if (parsed.has('c')) {
        n = parse_count(parsed.get('c').value_or("0"), from_start);
        use_bytes = true;
        use_lines = false;
    }

    const auto& pos = parsed.positional();

    // Check for +N positional arg
    std::vector<std::string_view> files;
    for (const auto& p : pos) {
        if (!p.empty() && p[0] == '+' && use_lines && !parsed.has('n')) {
            from_start = true;
            n = std::strtol(std::string{p.substr(1)}.c_str(), nullptr, 10);
        } else {
            files.push_back(p);
        }
    }

    bool multi = files.size() > 1;

    if (files.empty()) {
        return tail_file("-", n, use_bytes, from_start, false);
    }

    int rc = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (tail_file(files[i], n, use_bytes, from_start, multi) != 0) rc = 1;
    }
    return rc;
}
