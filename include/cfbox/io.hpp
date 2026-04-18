#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/error.hpp>

namespace cfbox::io {

inline auto read_all(std::string_view path) -> base::Result<std::string> {
    std::FILE* f = std::fopen(std::string{path}.c_str(), "rb");
    if (!f) {
        return std::unexpected(base::Error{errno, "cannot open file: " + std::string{path}});
    }

    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::string content(static_cast<std::size_t>(size), '\0');
    auto nread = std::fread(content.data(), 1, content.size(), f);
    content.resize(nread);
    std::fclose(f);
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
    std::FILE* f = std::fopen(std::string{path}.c_str(), "wb");
    if (!f) {
        return std::unexpected(
            base::Error{errno, "cannot open file for writing: " + std::string{path}});
    }
    auto written = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (written != data.size()) {
        return std::unexpected(base::Error{errno, "write failed: " + std::string{path}});
    }
    return {};
}

} // namespace cfbox::io
