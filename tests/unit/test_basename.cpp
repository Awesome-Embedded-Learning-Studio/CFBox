#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_BASENAME

using namespace cfbox::test;

TEST(BasenameTest, Basic) {
    char a0[] = "basename", a1[] = "/foo/bar";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return basename_main(2, argv); });
    EXPECT_EQ(out, "bar\n");
}

TEST(BasenameTest, TrailingSlash) {
    char a0[] = "basename", a1[] = "/foo/bar/";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return basename_main(2, argv); });
    EXPECT_EQ(out, "bar\n");
}

TEST(BasenameTest, NoDirectory) {
    char a0[] = "basename", a1[] = "bar";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return basename_main(2, argv); });
    EXPECT_EQ(out, "bar\n");
}

TEST(BasenameTest, Root) {
    char a0[] = "basename", a1[] = "/";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return basename_main(2, argv); });
    EXPECT_EQ(out, "/\n");
}

TEST(BasenameTest, WithSuffix) {
    char a0[] = "basename", a1[] = "/foo/bar.c", a2[] = ".c";
    char* argv[] = {a0, a1, a2, nullptr};
    auto out = capture_stdout([&] { return basename_main(3, argv); });
    EXPECT_EQ(out, "bar\n");
}

TEST(BasenameTest, SuffixNotMatched) {
    char a0[] = "basename", a1[] = "/foo/bar", a2[] = ".c";
    char* argv[] = {a0, a1, a2, nullptr};
    auto out = capture_stdout([&] { return basename_main(3, argv); });
    EXPECT_EQ(out, "bar\n");
}

TEST(BasenameTest, SuffixEqualsName) {
    char a0[] = "basename", a1[] = "/foo/.c", a2[] = ".c";
    char* argv[] = {a0, a1, a2, nullptr};
    auto out = capture_stdout([&] { return basename_main(3, argv); });
    EXPECT_EQ(out, ".c\n");
}

TEST(BasenameTest, MissingOperand) {
    char a0[] = "basename";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(basename_main(1, argv), 1);
}

#endif // CFBOX_ENABLE_BASENAME
