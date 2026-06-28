#pragma once
#include <cstdio>
#include <expected>
#include <string>

namespace cfbox::base {

struct Error {
    int code;
    std::string msg;
};

template <typename T> using Result = std::expected<T, Error>;

} // namespace cfbox::base

#ifndef CFBOX_ERR
#    define CFBOX_ERR(cmd, fmt, ...) \
        std::fprintf(stderr, "cfbox " cmd ": " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#    define CFBOX_ERR_V(cmd, fmt, ...) \
        std::fprintf(stderr, "cfbox %s: " fmt "\n", cmd __VA_OPT__(,) __VA_ARGS__)
#endif

#ifndef CFBOX_TRY
#    define CFBOX_TRY(var, expr)                                        \
        auto var = (expr);                                              \
        if (!var) {                                                     \
            return std::unexpected(std::move(var).error());             \
        }
#endif
