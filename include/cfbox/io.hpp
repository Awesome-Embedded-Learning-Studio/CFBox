#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include <cfbox/error.hpp>

namespace cfbox::io {

struct FileCloser {
    void operator()(std::FILE* f) const noexcept {
        if (f)
            std::fclose(f);
    }
};
using unique_file = std::unique_ptr<std::FILE, FileCloser>;

// RAII wrapper for a raw POSIX file descriptor. Not unique_ptr<int, ...>: a
// descriptor is a value, not a pointer — no heap alloc, every method noexcept.
class unique_fd {
public:
    explicit unique_fd(int fd = -1) noexcept : fd_{fd} {}
    ~unique_fd() noexcept { reset(); }
    unique_fd(unique_fd&& other) noexcept : fd_{other.release()} {}
    auto operator=(unique_fd&& other) noexcept -> unique_fd& {
        reset(other.release());
        return *this;
    }
    unique_fd(const unique_fd&) = delete;
    auto operator=(const unique_fd&) = delete;

    [[nodiscard]] auto get() const noexcept -> int { return fd_; }
    [[nodiscard]] auto release() noexcept -> int {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }
    auto reset(int fd = -1) noexcept -> void {
        if (fd_ >= 0) ::close(fd_);
        fd_ = fd;
    }
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }

private:
    int fd_{-1};
};

[[nodiscard]] inline auto open_file(std::string_view path, const char* mode)
    -> base::Result<unique_file> {
    auto* f = std::fopen(path.data(), mode);
    if (!f) {
        return std::unexpected(base::Error{errno, "cannot open file: " + std::string{path}});
    }
    return unique_file{f};
}

[[nodiscard]] inline auto read_all(std::string_view path) -> base::Result<std::string> {
    CFBOX_TRY(f, open_file(path, "rb"));

    std::fseek(f->get(), 0, SEEK_END);
    long size = std::ftell(f->get());
    if (size > 0) {
        std::fseek(f->get(), 0, SEEK_SET);
        std::string content(static_cast<std::size_t>(size), '\0');
        auto nread = std::fread(content.data(), 1, content.size(), f->get());
        content.resize(nread);
        return content;
    }

    // Virtual files (e.g. /proc) may report 0 size; fall back to incremental read
    std::fseek(f->get(), 0, SEEK_SET);
    std::string content;
    char buf[4096];
    while (auto n = std::fread(buf, 1, sizeof(buf), f->get())) {
        content.append(buf, n);
    }
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
    if (content.empty())
        return lines;

    auto nl = static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n'));
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

[[nodiscard]] inline auto read_lines(std::string_view path)
    -> base::Result<std::vector<std::string>> {
    CFBOX_TRY(content, read_all(path));
    auto lines = split_lines(*content);
    if (content->empty())
        lines.emplace_back();
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

template <typename Fn> auto for_each_line(std::FILE* f, Fn&& fn) -> base::Result<void> {
    // Deliver a complete line; returns false to stop early (only when fn returns bool).
    auto deliver = [&fn](const std::string& line) -> bool {
        if constexpr (std::is_invocable_r_v<bool, Fn, const std::string&>) {
            return fn(line);
        } else {
            fn(line);
            return true;
        }
    };

    // Block reads + memchr for newlines instead of fgetc-per-byte. Same line
    // semantics (split on '\n', a trailing line without '\n' is still delivered,
    // ferror checked, bool early-return honored); the buffer is heap-allocated so
    // the stack stays tiny even with a large chunk.
    constexpr std::size_t kChunk = 65536;
    std::vector<char> buf(kChunk);
    std::string pending;  // partial line carried across block boundaries
    std::size_t pos = 0;
    std::size_t avail = 0;

    for (;;) {
        if (pos == avail) {
            avail = std::fread(buf.data(), 1, kChunk, f);
            pos = 0;
            if (avail == 0) break;
        }
        if (auto* nl = static_cast<char*>(std::memchr(buf.data() + pos, '\n', avail - pos))) {
            std::string line;
            if (!pending.empty()) line = std::move(pending);
            line.append(buf.data() + pos, static_cast<std::size_t>(nl - (buf.data() + pos)));
            pos = static_cast<std::size_t>(nl - buf.data()) + 1;
            if (!deliver(line)) return {};
        } else {
            pending.append(buf.data() + pos, avail - pos);
            pos = avail;
        }
    }
    if (std::ferror(f)) {
        return std::unexpected(base::Error{errno, "read error"});
    }
    if (!pending.empty()) {
        deliver(pending);  // trailing line without newline (bool ignored, as before)
    }
    return {};
}

template <typename Fn> auto for_each_line(std::string_view path, Fn&& fn) -> base::Result<void> {
    if (path == "-") {
        return for_each_line(stdin, std::forward<Fn>(fn));
    }
    CFBOX_TRY(f, open_file(path, "r"));
    return for_each_line(f->get(), std::forward<Fn>(fn));
}

} // namespace cfbox::io
