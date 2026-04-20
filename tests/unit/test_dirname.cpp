#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_DIRNAME

using namespace cfbox::test;

TEST(DirnameTest, Basic) {
    char a0[] = "dirname", a1[] = "/foo/bar";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, "/foo\n");
}

TEST(DirnameTest, TrailingSlash) {
    char a0[] = "dirname", a1[] = "/foo/bar/";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, "/foo\n");
}

TEST(DirnameTest, NoDirectory) {
    char a0[] = "dirname", a1[] = "bar";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, ".\n");
}

TEST(DirnameTest, RootChild) {
    char a0[] = "dirname", a1[] = "/foo";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, "/\n");
}

TEST(DirnameTest, Root) {
    char a0[] = "dirname", a1[] = "/";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, "/\n");
}

TEST(DirnameTest, Dot) {
    char a0[] = "dirname", a1[] = ".";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, ".\n");
}

TEST(DirnameTest, DotDot) {
    char a0[] = "dirname", a1[] = "..";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return dirname_main(2, argv); });
    EXPECT_EQ(out, ".\n");
}

TEST(DirnameTest, MissingOperand) {
    char a0[] = "dirname";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(dirname_main(1, argv), 1);
}

#endif // CFBOX_ENABLE_DIRNAME
