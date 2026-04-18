#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(UniqTest, BasicDedup) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "apple\napple\nbanana\nbanana\nbanana\ncherry\n");
    char a0[] = "uniq", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return uniq_main(2, argv); });
    EXPECT_EQ(out, "apple\nbanana\ncherry\n");
}

TEST(UniqTest, CountFlag) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\na\na\nb\nb\nc\n");
    char a0[] = "uniq", a1[] = "-c", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return uniq_main(3, argv); });
    EXPECT_EQ(out, "      3 a\n      2 b\n      1 c\n");
}

TEST(UniqTest, RepeatedOnly) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\na\nb\nc\nc\n");
    char a0[] = "uniq", a1[] = "-d", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return uniq_main(3, argv); });
    EXPECT_EQ(out, "a\nc\n");
}

TEST(UniqTest, UniqueOnly) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\na\nb\nc\nc\n");
    char a0[] = "uniq", a1[] = "-u", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return uniq_main(3, argv); });
    EXPECT_EQ(out, "b\n");
}

TEST(UniqTest, NoDuplicates) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\nb\nc\n");
    char a0[] = "uniq", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return uniq_main(2, argv); });
    EXPECT_EQ(out, "a\nb\nc\n");
}

TEST(UniqTest, EmptyFile) {
    TempDir tmp;
    auto f = tmp.write_file("empty.txt", "");
    char a0[] = "uniq", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return uniq_main(2, argv); });
    EXPECT_EQ(out, "");
}
