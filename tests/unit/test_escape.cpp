#include <cfbox/escape.hpp>
#include <gtest/gtest.h>

using namespace cfbox::util;

TEST(EscapeTest, PlainString) {
    EXPECT_EQ(process_escape("hello"), "hello");
}

TEST(EscapeTest, Newline) {
    EXPECT_EQ(process_escape("a\\nb"), "a\nb");
}

TEST(EscapeTest, Tab) {
    EXPECT_EQ(process_escape("a\\tb"), "a\tb");
}

TEST(EscapeTest, Backslash) {
    EXPECT_EQ(process_escape("a\\\\b"), "a\\b");
}

TEST(EscapeTest, Alert) {
    EXPECT_EQ(process_escape("\\a"), "\a");
}

TEST(EscapeTest, Backspace) {
    EXPECT_EQ(process_escape("\\b"), "\b");
}

TEST(EscapeTest, CarriageReturn) {
    EXPECT_EQ(process_escape("\\r"), "\r");
}

TEST(EscapeTest, FormFeed) {
    EXPECT_EQ(process_escape("\\f"), "\f");
}

TEST(EscapeTest, VerticalTab) {
    EXPECT_EQ(process_escape("\\v"), "\v");
}

TEST(EscapeTest, Octal) {
    // \014 = octal 14 = decimal 12 = \f
    EXPECT_EQ(process_escape("\\014"), "\f");
    EXPECT_EQ(process_escape("\\0101"), "A"); // octal 101 = 'A'
}

TEST(EscapeTest, MultipleEscapes) {
    EXPECT_EQ(process_escape("\\n\\t"), "\n\t");
}

TEST(EscapeTest, TrailingBackslash) {
    EXPECT_EQ(process_escape("abc\\"), "abc\\");
}

TEST(EscapeTest, Empty) {
    EXPECT_EQ(process_escape(""), "");
}
