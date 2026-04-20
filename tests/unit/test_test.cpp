#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
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

#endif // CFBOX_ENABLE_TEST
