#pragma once

#include <cstddef>
#include <string_view>

namespace cfbox::utf8 {

constexpr auto is_continuation(unsigned char b) noexcept -> bool {
    return (b & 0xC0) == 0x80;
}

struct DecodeResult {
    char32_t code_point;
    std::size_t bytes_consumed;
};

[[nodiscard]] constexpr auto decode(std::string_view str, std::size_t pos) noexcept -> DecodeResult {
    if (pos >= str.size()) return {char32_t(0), 0};

    unsigned char b0 = static_cast<unsigned char>(str[pos]);

    // Single byte (0xxxxxxx)
    if (b0 < 0x80) return {char32_t(b0), 1};

    // Invalid: continuation byte as lead
    if (is_continuation(b0)) return {char32_t(0xFFFD), 1};

    // Determine sequence length and initial bits
    std::size_t len = 0;
    char32_t cp = 0;

    if ((b0 & 0xE0) == 0xC0) { len = 2; cp = char32_t(b0 & 0x1F); }
    else if ((b0 & 0xF0) == 0xE0) { len = 3; cp = char32_t(b0 & 0x0F); }
    else if ((b0 & 0xF8) == 0xF0) { len = 4; cp = char32_t(b0 & 0x07); }
    else return {char32_t(0xFFFD), 1}; // 0xF8..0xFF invalid

    if (pos + len > str.size()) return {char32_t(0xFFFD), 1};

    for (std::size_t i = 1; i < len; ++i) {
        unsigned char b = static_cast<unsigned char>(str[pos + i]);
        if (!is_continuation(b)) return {char32_t(0xFFFD), 1};
        cp = (cp << 6) | char32_t(b & 0x3F);
    }

    // Reject overlong encodings and surrogates
    if (len == 2 && cp < 0x80) return {char32_t(0xFFFD), 1};
    if (len == 3 && cp < 0x800) return {char32_t(0xFFFD), 1};
    if (len == 4 && cp < 0x10000) return {char32_t(0xFFFD), 1};
    if (cp >= 0xD800 && cp <= 0xDFFF) return {char32_t(0xFFFD), len};
    if (cp > 0x10FFFF) return {char32_t(0xFFFD), len};

    return {cp, len};
}

[[nodiscard]] constexpr auto count_code_points(std::string_view str) noexcept -> std::size_t {
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos < str.size()) {
        auto [cp, consumed] = decode(str, pos);
        if (consumed == 0) break;
        (void)cp;
        ++count;
        pos += consumed;
    }
    return count;
}

[[nodiscard]] constexpr auto char_width(char32_t cp) noexcept -> int {
    // Control characters
    if (cp < 0x20) return 0;
    // DEL
    if (cp == 0x7F) return 0;
    // ASCII printable
    if (cp < 0x80) return 1;

    // CJK Unified Ideographs
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 2;
    // CJK Compatibility Ideographs
    if (cp >= 0xF900 && cp <= 0xFAFF) return 2;
    // CJK Extensions A+B
    if (cp >= 0x3400 && cp <= 0x4DBF) return 2;
    // Halfwidth and Fullwidth Forms
    if (cp >= 0xFF01 && cp <= 0xFF60) return 2;
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return 2;
    // Hangul Syllables
    if (cp >= 0xAC00 && cp <= 0xD7AF) return 2;
    // Hangul Jamo
    if (cp >= 0x1100 && cp <= 0x11FF) return 2;
    // Katakana + Hiragana
    if (cp >= 0x3040 && cp <= 0x309F) return 2; // Hiragana
    if (cp >= 0x30A0 && cp <= 0x30FF) return 2; // Katakana
    if (cp >= 0x31F0 && cp <= 0x31FF) return 2; // Katakana Phonetic Extensions
    // Bopomofo
    if (cp >= 0x3100 && cp <= 0x312F) return 2;
    if (cp >= 0x31A0 && cp <= 0x31BF) return 2;
    // Fullwidth digits/letters
    if (cp >= 0xFF10 && cp <= 0xFF19) return 2; // fullwidth digits
    if (cp >= 0xFF21 && cp <= 0xFF3A) return 2; // fullwidth upper
    if (cp >= 0xFF41 && cp <= 0xFF5A) return 2; // fullwidth lower
    // Various CJK range
    if (cp >= 0x2E80 && cp <= 0x2EFF) return 2; // CJK Radicals Supplement
    if (cp >= 0x2F00 && cp <= 0x2FDF) return 2; // Kangxi Radicals
    if (cp >= 0x3000 && cp <= 0x303F) return 2; // CJK Symbols and Punctuation
    // Combining marks — width 0
    if (cp >= 0x0300 && cp <= 0x036F) return 0;
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return 0;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return 0;
    if (cp >= 0x20D0 && cp <= 0x20FF) return 0;
    if (cp >= 0xFE20 && cp <= 0xFE2F) return 0;

    return 1;
}

[[nodiscard]] constexpr auto display_width(std::string_view str) noexcept -> std::size_t {
    std::size_t width = 0;
    std::size_t pos = 0;
    while (pos < str.size()) {
        auto [cp, consumed] = decode(str, pos);
        if (consumed == 0) break;
        if (cp == '\t') {
            // Tab stops at every 8 columns
            width = (width / 8 + 1) * 8;
        } else {
            width += static_cast<std::size_t>(char_width(cp));
        }
        pos += consumed;
    }
    return width;
}

[[nodiscard]] constexpr auto truncate_width(std::string_view str, std::size_t max_width) noexcept -> std::string_view {
    std::size_t width = 0;
    std::size_t pos = 0;
    while (pos < str.size()) {
        auto [cp, consumed] = decode(str, pos);
        if (consumed == 0) break;

        int cw = (cp == '\t') ? static_cast<int>(((width / 8) + 1) * 8 - width) : char_width(cp);
        if (width + static_cast<std::size_t>(cw) > max_width) {
            return str.substr(0, pos);
        }
        width += static_cast<std::size_t>(cw);
        pos += consumed;
    }
    return str;
}

} // namespace cfbox::utf8

// Compile-time verification
static_assert(cfbox::utf8::char_width(U'A') == 1);
static_assert(cfbox::utf8::char_width(U'中') == 2);
static_assert(cfbox::utf8::is_continuation(0x80));
static_assert(!cfbox::utf8::is_continuation(0x41));
static_assert(cfbox::utf8::count_code_points("abc") == 3);
