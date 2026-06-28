#include <cstdio>
#include <time.h>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <cfbox/fs_util.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_TEST

using namespace cfbox::test;

TEST(TestApplet, EmptyIsFalse) {
    char a0[] = "test";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(test_main(1, argv), 1);
}

TEST(TestApplet, NonEmptyStringIsTrue) {
    char a0[] = "test", a1[] = "hello";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(test_main(2, argv), 0);
}

TEST(TestApplet, EmptyStringIsFalse) {
    char a0[] = "test", a1[] = "";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(test_main(2, argv), 1);
}

TEST(TestApplet, ZFlag) {
    char a0[] = "test", a1[] = "-z", a2[] = "";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 0);

    char b2[] = "abc";
    argv[2] = b2;
    EXPECT_EQ(test_main(3, argv), 1);
}

TEST(TestApplet, NFlag) {
    char a0[] = "test", a1[] = "-n", a2[] = "abc";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 0);

    char b2[] = "";
    argv[2] = b2;
    EXPECT_EQ(test_main(3, argv), 1);
}

TEST(TestApplet, StringEquals) {
    char a0[] = "test", a1[] = "abc", a2[] = "=", a3[] = "abc";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);
}

TEST(TestApplet, StringNotEquals) {
    char a0[] = "test", a1[] = "abc", a2[] = "!=", a3[] = "def";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);
}

TEST(TestApplet, IntEq) {
    char a0[] = "test", a1[] = "1", a2[] = "-eq", a3[] = "1";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);
}

TEST(TestApplet, IntLt) {
    char a0[] = "test", a1[] = "1", a2[] = "-lt", a3[] = "2";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);
}

TEST(TestApplet, FileExists) {
    char a0[] = "test", a1[] = "-e", a2[] = "/dev/null";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 0);
}

TEST(TestApplet, FileIsDir) {
    char a0[] = "test", a1[] = "-d", a2[] = "/tmp";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 0);
}

TEST(TestApplet, NotOperator) {
    char a0[] = "test", a1[] = "!", a2[] = "";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 0);
}

TEST(TestApplet, AndOperator) {
    char a0[] = "test", a1[] = "abc", a2[] = "=", a3[] = "abc", a4[] = "-a", a5[] = "1", a6[] = "-eq", a7[] = "1";
    char* argv[] = {a0, a1, a2, a3, a4, a5, a6, a7, nullptr};
    EXPECT_EQ(test_main(8, argv), 0);
}

TEST(TestApplet, OrOperator) {
    char a0[] = "test", a1[] = "abc", a2[] = "=", a3[] = "def", a4[] = "-o", a5[] = "1", a6[] = "-eq", a7[] = "1";
    char* argv[] = {a0, a1, a2, a3, a4, a5, a6, a7, nullptr};
    EXPECT_EQ(test_main(8, argv), 0);
}

// --- integer validation: invalid operands must exit 2, not silently coerce ---
TEST(TestApplet, IntInvalidNonNumeric) {
    char a0[] = "test", a1[] = "abc", a2[] = "-eq", a3[] = "abc";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 2);
}

TEST(TestApplet, IntInvalidTrailingJunk) {
    char a0[] = "test", a1[] = "5", a2[] = "-eq", a3[] = "5x";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 2);
}

// --- operand/arity errors -> exit 2 ---
TEST(TestApplet, BareUnaryNoOperand) {
    char a0[] = "test", a1[] = "-z";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(test_main(2, argv), 2);
}

TEST(TestApplet, UnknownOperator) {
    char a0[] = "test", a1[] = "-q", a2[] = "foo";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 2);
}

TEST(TestApplet, UnmatchedOpenParen) {
    char a0[] = "test", a1[] = "(", a2[] = "a";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 2);
}

TEST(TestApplet, StrayCloseParen) {
    char a0[] = "test", a1[] = "a", a2[] = ")";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(test_main(3, argv), 2);
}

// --- trailing "!" is a non-empty string operand (POSIX: `test !` -> true) ---
TEST(TestApplet, TrailingBangIsString) {
    char a0[] = "test", a1[] = "!";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(test_main(2, argv), 0);
}

// --- new operators: string < >, file -nt/-ef ---
TEST(TestApplet, StringLessThan) {
    char a0[] = "test", a1[] = "a", a2[] = "<", a3[] = "b";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);
}

TEST(TestApplet, NestedOrInParens) {
    char a0[] = "test", a1[] = "(", a2[] = "a", a3[] = "=", a4[] = "a",
         a5[] = "-o", a6[] = "b", a7[] = "=", a8[] = "c", a9[] = ")";
    char* argv[] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, nullptr};
    EXPECT_EQ(test_main(10, argv), 0);  // (a=a OR b=c) -> true
}

TEST(TestApplet, EfSameFile) {
    TempDir tmp;
    auto f = tmp.write_file("a.txt", "x");
    char a0[] = "test";
    char a1[256], a2[] = "-ef", a3[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);  // same file -ef -> true
}

TEST(TestApplet, NewerThan) {
    TempDir tmp;
    auto old_f = tmp.write_file("old.txt", "x");
    auto new_f = tmp.write_file("new.txt", "y");
    struct timespec past[2] = {{1, 0}, {1, 0}};
    auto r = cfbox::fs::set_times(old_f, past, /*no_follow=*/false);
    (void)r;
    char a0[] = "test";
    char a1[256], a2[] = "-nt", a3[256];
    std::snprintf(a1, sizeof(a1), "%s", new_f.c_str());
    std::snprintf(a3, sizeof(a3), "%s", old_f.c_str());
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(test_main(4, argv), 0);  // new -nt old -> true
}

#endif // CFBOX_ENABLE_TEST
