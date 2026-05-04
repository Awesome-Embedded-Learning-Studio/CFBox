#pragma once

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/error.hpp>

namespace cfbox::io {

struct FileCloser {
    void operator()(std::FILE* f) const noexcept {
        if (f) std::fclose(f);
    }
};
using unique_file = std::unique_ptr<std::FILE, FileCloser>;

[[nodiscard]] inline auto open_file(std::string_view path, const char* mode) -> base::Result<unique_file> {
    auto* f = std::fopen(std::string{path}.c_str(), mode);
    if (!f) {
        return std::unexpected(base::Error{errno, "cannot open file: " + std::string{path}});
    }
    return unique_file{f};
}

[[nodiscard]] inline auto read_all(std::string_view path) -> base::Result<std::string> {
    CFBOX_TRY(f, open_file(path, "rb"));

    std::fseek(f->get(), 0, SEEK_END);
    long size = std::ftell(f->get());
    std::fseek(f->get(), 0, SEEK_SET);

    std::string content(static_cast<std::size_t>(size), '\0');
    auto nread = std::fread(content.data(), 1, content.size(), f->get());
    content.resize(nread);
    return content;
}

[[nodiscard]] inline auto read_all_stdin() -> base::Result<std::string> {
    std::string content;
    char buf[4096];
    while (auto n = std::fread(buf, 1, sizeof(buf), stdin)) {
        content.append(buf, n);
    }
    return content;
}

[[nodiscard]] inline auto split_lines(std::string_view content) -> std::vector<std::string> {
    std::vector<std::string> lines;
    if (content.empty()) return lines;

    auto nl = static_cast<std::size_t>(
        std::count(content.begin(), content.end(), '\n'));
    lines.reserve(nl + 1);

    std::size_t start = 0;
    while (start < content.size()) {
        auto pos = content.find('\n', start);
        if (pos == std::string_view::npos) {
            lines.emplace_back(content.substr(start));
            break;
        }
        lines.emplace_back(content.substr(start, pos - start));
        start = pos + 1;
    }
    return lines;
}

[[nodiscard]] inline auto read_lines(std::string_view path) -> base::Result<std::vector<std::string>> {
    CFBOX_TRY(content, read_all(path));
    auto lines = split_lines(*content);
    if (content->empty()) lines.emplace_back();
    return lines;
}

inline auto write_all(std::string_view path, std::string_view data) -> base::Result<void> {
    CFBOX_TRY(f, open_file(path, "wb"));
    auto written = std::fwrite(data.data(), 1, data.size(), f->get());
    if (written != data.size()) {
        return std::unexpected(base::Error{errno, "write failed: " + std::string{path}});
    }
    return {};
}

template <typename Fn>
auto for_each_line(std::FILE* f, Fn&& fn) -> base::Result<void> {
    std::string line;
    line.reserve(256);
    int ch;
    while ((ch = std::fgetc(f)) != EOF) {
        if (ch == '\n') {
            if constexpr (std::is_invocable_r_v<bool, Fn, const std::string&>) {
                if (!fn(line)) return {};
            } else {
                fn(line);
            }
            line.clear();
        } else {
            line += static_cast<char>(ch);
        }
    }
    if (std::ferror(f)) {
        return std::unexpected(base::Error{errno, "read error"});
    }
    if (!line.empty()) {
        if constexpr (std::is_invocable_r_v<bool, Fn, const std::string&>) {
            fn(line);
        } else {
            fn(line);
        }
    }
    return {};
}

template <typename Fn>
auto for_each_line(std::string_view path, Fn&& fn) -> base::Result<void> {
    if (path == "-") {
        return for_each_line(stdin, std::forward<Fn>(fn));
    }
    CFBOX_TRY(f, open_file(path, "r"));
    return for_each_line(f->get(), std::forward<Fn>(fn));
}

} // namespace cfbox::io
