#pragma once

#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace cfbox::args {

struct OptSpec {
    char flag;
    bool has_value; // true = flag takes a value (e.g. -n 10)
};

class ParseResult {
    std::vector<char> flags_;
    std::vector<std::pair<char, std::string_view>> values_;
    std::vector<std::string_view> positional_;

    friend auto parse(int argc, char* argv[],
                      std::initializer_list<OptSpec> specs) -> ParseResult;

public:
    [[nodiscard]] auto has(char flag) const -> bool {
        for (char c : flags_)
            if (c == flag) return true;
        for (const auto& [c, _] : values_)
            if (c == flag) return true;
        return false;
    }

    [[nodiscard]] auto get(char flag) const -> std::optional<std::string_view> {
        for (const auto& [c, v] : values_)
            if (c == flag) return v;
        return std::nullopt;
    }

    [[nodiscard]] auto positional() const -> const std::vector<std::string_view>& {
        return positional_;
    }
};

inline auto parse(int argc, char* argv[],
                  std::initializer_list<OptSpec> specs) -> ParseResult {
    ParseResult result;
    bool stop = false;

    auto takes_value = [&](char c) {
        for (const auto& s : specs)
            if (s.flag == c) return s.has_value;
        return false;
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        // positional if: stopped, not a flag, or bare "-"
        if (stop || arg.empty() || arg[0] != '-' || arg.size() == 1) {
            result.positional_.push_back(arg);
            continue;
        }

        if (arg == "--") {
            stop = true;
            continue;
        }

        for (std::size_t j = 1; j < arg.size(); ++j) {
            const char c = arg[j];

            if (takes_value(c)) {
                // rest of this arg, or next arg
                if (j + 1 < arg.size()) {
                    result.values_.emplace_back(c, arg.substr(j + 1));
                } else if (i + 1 < argc) {
                    result.values_.emplace_back(c, std::string_view{argv[++i]});
                } else {
                    result.values_.emplace_back(c, std::string_view{});
                }
                break; // rest of arg consumed as value
            }

            result.flags_.push_back(c);
        }
    }

    return result;
}

} // namespace cfbox::args
