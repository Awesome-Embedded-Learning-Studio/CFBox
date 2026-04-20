#pragma once

#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace cfbox::args {

struct OptSpec {
    char flag;
    bool has_value;               // true = flag takes a value (e.g. -n 10)
    std::string_view long_name{}; // e.g. "recursive", empty = no long option
};

class ParseResult {
    std::vector<char> flags_;
    std::vector<std::pair<char, std::string_view>> values_;
    std::vector<std::pair<std::string_view, std::optional<std::string_view>>> long_opts_;
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

    // Long option queries
    [[nodiscard]] auto has_long(std::string_view name) const -> bool {
        for (const auto& [n, _] : long_opts_)
            if (n == name) return true;
        return false;
    }

    [[nodiscard]] auto get_long(std::string_view name) const
        -> std::optional<std::string_view> {
        for (const auto& [n, v] : long_opts_)
            if (n == name) return v;
        return std::nullopt;
    }

    // Check either short or long form
    [[nodiscard]] auto has_any(char flag, std::string_view long_name) const -> bool {
        return has(flag) || has_long(long_name);
    }

    [[nodiscard]] auto get_any(char flag, std::string_view long_name) const
        -> std::optional<std::string_view> {
        if (auto v = get(flag)) return v;
        return get_long(long_name);
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

    auto find_long_spec = [&](std::string_view name) -> const OptSpec* {
        for (const auto& s : specs)
            if (s.long_name == name) return &s;
        return nullptr;
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

        // Long option: starts with "--" and has more than 2 chars
        if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
            std::string_view long_arg = arg.substr(2);
            std::string_view name = long_arg;
            std::optional<std::string_view> value;

            // Check for --name=value
            auto eq_pos = long_arg.find('=');
            if (eq_pos != std::string_view::npos) {
                name = long_arg.substr(0, eq_pos);
                value = long_arg.substr(eq_pos + 1);
            }

            const OptSpec* spec = find_long_spec(name);

            if (spec && spec->has_value && !value) {
                // --name value (separate arg)
                if (i + 1 < argc) {
                    value = std::string_view{argv[++i]};
                }
            }

            // Store in both short and long for matched specs
            if (spec && value) {
                result.values_.emplace_back(spec->flag, *value);
            } else if (spec && !spec->has_value) {
                result.flags_.push_back(spec->flag);
            }

            // Always store the long option entry
            result.long_opts_.emplace_back(name, value);
            continue;
        }

        // Short option(s): -abc
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
