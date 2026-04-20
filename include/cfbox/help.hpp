#pragma once

#include <cstdio>
#include <string_view>

#include <cfbox/term.hpp>

#ifndef CFBOX_VERSION_STRING
#define CFBOX_VERSION_STRING "0.0.1"
#endif

namespace cfbox::help {

struct HelpEntry {
    std::string_view name;
    std::string_view version;
    std::string_view one_line;
    std::string_view usage;
    std::string_view options;
    std::string_view extra;
};

namespace detail {
inline void write_sv(std::string_view sv) {
    if (!sv.empty()) std::fwrite(sv.data(), 1, sv.size(), stdout);
}
} // namespace detail

inline auto print_help(const HelpEntry& entry) -> void {
    // Title: "cfbox <name> -- <one_line>"
    detail::write_sv(term::bold());
    std::fwrite("cfbox ", 1, 6, stdout);
    detail::write_sv(entry.name);
    detail::write_sv(term::reset());
    std::fwrite(" -- ", 1, 4, stdout);
    detail::write_sv(entry.one_line);
    std::fputc('\n', stdout);
    std::fputc('\n', stdout);

    // Usage
    detail::write_sv(term::bold());
    std::fwrite("Usage:", 1, 6, stdout);
    detail::write_sv(term::reset());
    std::fputc('\n', stdout);
    std::fputc(' ', stdout);
    std::fputc(' ', stdout);
    detail::write_sv(entry.usage);
    std::fputc('\n', stdout);
    std::fputc('\n', stdout);

    // Options
    if (!entry.options.empty()) {
        detail::write_sv(term::bold());
        std::fwrite("Options:", 1, 8, stdout);
        detail::write_sv(term::reset());
        std::fputc('\n', stdout);
        detail::write_sv(entry.options);
        std::fputc('\n', stdout);
    }

    // Auto-append --help and --version
    std::fwrite("      ", 1, 6, stdout);
    detail::write_sv(term::cyan());
    std::fwrite("--help", 1, 6, stdout);
    detail::write_sv(term::reset());
    std::fwrite("     display this help and exit\n", 1, 32, stdout);

    std::fwrite("      ", 1, 6, stdout);
    detail::write_sv(term::cyan());
    std::fwrite("--version", 1, 9, stdout);
    detail::write_sv(term::reset());
    std::fwrite("  output version information and exit\n", 1, 38, stdout);

    // Extra notes
    if (!entry.extra.empty()) {
        std::fputc('\n', stdout);
        detail::write_sv(entry.extra);
        std::fputc('\n', stdout);
    }
}

inline auto print_version(const HelpEntry& entry) -> void {
    std::fwrite("cfbox ", 1, 6, stdout);
    detail::write_sv(entry.name);
    std::fputc(' ', stdout);
    detail::write_sv(entry.version);
    std::fputc('\n', stdout);
}

inline auto print_cfbox_version() -> void {
    std::fwrite("cfbox ", 1, 6, stdout);
    std::fwrite(CFBOX_VERSION_STRING, 1, sizeof(CFBOX_VERSION_STRING) - 1, stdout);
    std::fputc('\n', stdout);
}

} // namespace cfbox::help
