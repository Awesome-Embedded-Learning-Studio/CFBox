#pragma once

#include <string>
#include <string_view>

namespace cfbox::util {

inline auto process_escape(std::string_view s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case 'n':  out += '\n'; ++i; break;
            case 't':  out += '\t'; ++i; break;
            case '\\': out += '\\'; ++i; break;
            case 'a':  out += '\a'; ++i; break;
            case 'b':  out += '\b'; ++i; break;
            case 'r':  out += '\r'; ++i; break;
            case 'f':  out += '\f'; ++i; break;
            case 'v':  out += '\v'; ++i; break;
            case '0': {
                // \0NNN: up to 3 octal digits after the 0
                int val = 0;
                int digits = 0;
                std::size_t j = i + 2;
                while (digits < 3 && j < s.size() && s[j] >= '0' && s[j] <= '7') {
                    val = val * 8 + (s[j] - '0');
                    ++j;
                    ++digits;
                }
                out += static_cast<char>(val);
                i = j - 1;
                break;
            }
            default: out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace cfbox::util
