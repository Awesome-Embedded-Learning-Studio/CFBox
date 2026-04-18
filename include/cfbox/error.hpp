#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace cfbox::base {

struct Error {
    int code;
    std::string msg;
};

struct ErrorView {
    int code;
    std::string_view msg;
};

template <typename T> using Result = std::expected<T, Error>;

} // namespace cfbox::base

#ifndef CFBOX_TRY
#    define CFBOX_TRY(var, expr)                                        \
        auto var = (expr);                                              \
        if (!var) {                                                     \
            return std::unexpected(std::move(var).error());             \
        }
#endif
