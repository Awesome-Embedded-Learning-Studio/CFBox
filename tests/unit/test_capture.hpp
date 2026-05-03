#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <unistd.h>

#include <cfbox/io.hpp>

namespace cfbox::test {

// Capture stdout produced by fn(), return as string
inline auto capture_stdout(std::function<int()> fn) -> std::string {
    char tmpfile[] = "/tmp/cfbox_test_XXXXXX";
    int fd = mkstemp(tmpfile);

    std::fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    dup2(fd, STDOUT_FILENO);
    close(fd);

    fn();

    std::fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    auto result = cfbox::io::read_all(tmpfile);
    unlink(tmpfile);
    return result.value_or("");
}

// RAII temp directory — auto-creates on construction, auto-removes on destruction
struct TempDir {
    std::filesystem::path path;

    TempDir() {
        char tmpl[] = "/tmp/cfbox_testdir_XXXXXX";
        path = mkdtemp(tmpl);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    // Write a file inside the temp dir
    auto write_file(const std::string& name, const std::string& content) const -> std::string {
        auto fp = path / name;
        static_cast<void>(cfbox::io::write_all(fp.string(), content));
        return fp.string();
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

} // namespace cfbox::test
