#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/error.hpp>
#include <cfbox/io.hpp>

namespace cfbox::stream {

inline auto for_each_line(std::string_view path,
                          std::function<bool(const std::string&, std::size_t)> fn)
    -> base::Result<void> {
    std::size_t line_num = 0;
    return io::for_each_line(path, [&](const std::string& line) {
        return fn(line, line_num++);
    });
}

inline auto split_fields(const std::string& line, char delim) -> std::vector<std::string> {
    std::vector<std::string> fields;
    std::string field;
    for (char c : line) {
        if (c == delim) {
            fields.push_back(std::move(field));
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(std::move(field));
    return fields;
}

inline auto split_whitespace(const std::string& line) -> std::vector<std::string> {
    std::vector<std::string> fields;
    std::string field;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            if (!field.empty()) {
                fields.push_back(std::move(field));
                field.clear();
            }
        } else {
            field += c;
        }
    }
    if (!field.empty()) {
        fields.push_back(std::move(field));
    }
    return fields;
}

} // namespace cfbox::stream
