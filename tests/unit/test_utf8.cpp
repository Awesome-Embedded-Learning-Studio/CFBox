#include <gtest/gtest.h>

#include <cfbox/utf8.hpp>

TEST(Utf8Test, IsContinuation) {
    EXPECT_TRUE(cfbox::utf8::is_continuation(0x80));
    EXPECT_TRUE(cfbox::utf8::is_continuation(0xBF));
    EXPECT_FALSE(cfbox::utf8::is_continuation(0x7F));
    EXPECT_FALSE(cfbox::utf8::is_continuation(0xC0));
    EXPECT_FALSE(cfbox::utf8::is_continuation(0x00));
}

TEST(Utf8Test, DecodeAscii) {
    auto r = cfbox::utf8::decode("hello", 0);
    EXPECT_EQ(r.code_point, U'h');
    EXPECT_EQ(r.bytes_consumed, 1);
}

TEST(Utf8Test, DecodeTwoByte) {
    // U+00E9 = é = 0xC3 0xA9
    auto r = cfbox::utf8::decode("\xC3\xA9", 0);
    EXPECT_EQ(r.code_point, U'\u00E9');
    EXPECT_EQ(r.bytes_consumed, 2);
}

TEST(Utf8Test, DecodeThreeByteCJK) {
    // U+4E2D = 中 = 0xE4 0xB8 0xAD
    auto r = cfbox::utf8::decode("\xE4\xB8\xAD", 0);
    EXPECT_EQ(r.code_point, U'\u4E2D');
    EXPECT_EQ(r.bytes_consumed, 3);
}

TEST(Utf8Test, DecodeFourByteEmoji) {
    // U+1F600 = 😀 = 0xF0 0x9F 0x98 0x80
    auto r = cfbox::utf8::decode("\xF0\x9F\x98\x80", 0);
    EXPECT_EQ(r.code_point, U'\U0001F600');
    EXPECT_EQ(r.bytes_consumed, 4);
}

TEST(Utf8Test, DecodeInvalidContinuation) {
    auto r = cfbox::utf8::decode("\x80", 0);
    EXPECT_EQ(r.code_point, char32_t(0xFFFD));
    EXPECT_EQ(r.bytes_consumed, 1);
}

TEST(Utf8Test, CountCodePointsAscii) {
    EXPECT_EQ(cfbox::utf8::count_code_points("hello"), 5u);
    EXPECT_EQ(cfbox::utf8::count_code_points(""), 0u);
}

TEST(Utf8Test, CountCodePointsMixed) {
    // "café" = c a f é (4 code points)
    const char cafe[] = "caf\xc3\xa9";
    EXPECT_EQ(cfbox::utf8::count_code_points(cafe), 4u);
}

TEST(Utf8Test, CountCodePointsCJK) {
    // "中文" = 2 code points
    const char zh[] = "\xE4\xB8\xAD\xE6\x96\x87";
    EXPECT_EQ(cfbox::utf8::count_code_points(zh), 2u);
}

TEST(Utf8Test, CharWidthAscii) {
    EXPECT_EQ(cfbox::utf8::char_width(U'A'), 1);
    EXPECT_EQ(cfbox::utf8::char_width(U' '), 1);
}

TEST(Utf8Test, CharWidthControl) {
    EXPECT_EQ(cfbox::utf8::char_width(U'\n'), 0);
    EXPECT_EQ(cfbox::utf8::char_width(U'\t'), 0);
    EXPECT_EQ(cfbox::utf8::char_width(char32_t(0x7F)), 0); // DEL
}

TEST(Utf8Test, CharWidthCJK) {
    EXPECT_EQ(cfbox::utf8::char_width(U'\u4E2D'), 2); // 中
    EXPECT_EQ(cfbox::utf8::char_width(U'\u6587'), 2); // 文
}

TEST(Utf8Test, CharWidthCombining) {
    EXPECT_EQ(cfbox::utf8::char_width(U'\u0300'), 0); // combining grave accent
}

TEST(Utf8Test, DisplayWidthAscii) {
    EXPECT_EQ(cfbox::utf8::display_width("abc"), 3u);
}

TEST(Utf8Test, DisplayWidthMixed) {
    // "中abc" = 2 + 1 + 1 + 1 = 5
    const char s[] = "\xE4\xB8\xAD"
                     "abc";
    EXPECT_EQ(cfbox::utf8::display_width(s), 5u);
}

TEST(Utf8Test, TruncateWidthAscii) {
    EXPECT_EQ(cfbox::utf8::truncate_width("abcdef", 3), "abc");
    EXPECT_EQ(cfbox::utf8::truncate_width("abc", 5), "abc");
}

TEST(Utf8Test, TruncateWidthCJK) {
    // "中文abc" — truncate to width 4 should give "中文" (width 4)
    const char input[] = "\xE4\xB8\xAD\xE6\x96\x87"
                         "abc";
    const char expected[] = "\xE4\xB8\xAD\xE6\x96\x87";
    EXPECT_EQ(cfbox::utf8::truncate_width(input, 4), expected);
}

TEST(Utf8Test, TruncateWidthDoesNotSplitCodePoint) {
    // "中abc" — truncate to width 3 should give "中a" (2+1=3), not split the CJK char
    const char input[] = "\xE4\xB8\xAD"
                         "abc";
    const char expected[] = "\xE4\xB8\xAD"
                            "a";
    EXPECT_EQ(cfbox::utf8::truncate_width(input, 3), expected);
}
