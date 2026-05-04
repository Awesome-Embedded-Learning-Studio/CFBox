#pragma once

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

inline auto open_file(std::string_view path, const char* mode) -> base::Result<unique_file> {
    auto* f = std::fopen(std::string{path}.c_str(), mode);
    if (!f) {
        return std::unexpected(base::Error{errno, "cannot open file: " + std::string{path}});
    }
    return unique_file{f};
}

inline auto read_all(std::string_view path) -> base::Result<std::string> {
    CFBOX_TRY(f, open_file(path, "rb"));

    std::fseek(f->get(), 0, SEEK_END);
    long size = std::ftell(f->get());
    std::fseek(f->get(), 0, SEEK_SET);

    std::string content(static_cast<std::size_t>(size), '\0');
    auto nread = std::fread(content.data(), 1, content.size(), f->get());
    content.resize(nread);
    return content;
}

inline auto read_all_stdin() -> base::Result<std::string> {
    std::string content;
    char buf[4096];
    while (auto n = std::fread(buf, 1, sizeof(buf), stdin)) {
        content.append(buf, n);
    }
    return content;
}

inline auto read_lines(std::string_view path) -> base::Result<std::vector<std::string>> {
    CFBOX_TRY(content, read_all(path));

    std::vector<std::string> lines;
    std::string line;
    for (char c : *content) {
        if (c == '\n') {
            lines.push_back(std::move(line));
            line.clear();
        } else {
            line += c;
        }
    }
    // last line without trailing newline
    if (!line.empty() || content->empty()) {
        lines.push_back(std::move(line));
    }
    return lines;
}

inline auto split_lines(const std::string& content) -> std::vector<std::string> {
    std::vector<std::string> lines;
    std::string line;
    for (char c : content) {
        if (c == '\n') {
            lines.push_back(std::move(line));
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) lines.push_back(std::move(line));
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

} // namespace cfbox::io
