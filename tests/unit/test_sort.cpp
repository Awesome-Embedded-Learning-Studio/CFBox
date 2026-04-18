#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(SortTest, BasicLexicographic) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "banana\napple\ncherry\n");
    char a0[] = "sort", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return sort_main(2, argv); });
    EXPECT_EQ(out, "apple\nbanana\ncherry\n");
}

TEST(SortTest, Reverse) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "banana\napple\ncherry\n");
    char a0[] = "sort", a1[] = "-r", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sort_main(3, argv); });
    EXPECT_EQ(out, "cherry\nbanana\napple\n");
}

TEST(SortTest, NumericSort) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "10\n2\n1\n20\n3\n");
    char a0[] = "sort", a1[] = "-n", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sort_main(3, argv); });
    EXPECT_EQ(out, "1\n2\n3\n10\n20\n");
}

TEST(SortTest, Unique) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "b\na\na\nc\nb\n");
    char a0[] = "sort", a1[] = "-u", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sort_main(3, argv); });
    EXPECT_EQ(out, "a\nb\nc\n");
}

TEST(SortTest, KeyField) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "b a\nc b\na c\n");
    char a0[] = "sort", a1[] = "-k", a2[] = "2", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return sort_main(4, argv); });
    EXPECT_EQ(out, "b a\nc b\na c\n");
}

TEST(SortTest, MultipleFiles) {
    TempDir tmp;
    auto f1 = tmp.write_file("a.txt", "banana\napple\n");
    auto f2 = tmp.write_file("b.txt", "cherry\napple\n");
    char a0[] = "sort", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", f1.c_str());
    std::snprintf(a2, sizeof(a2), "%s", f2.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sort_main(3, argv); });
    EXPECT_EQ(out, "apple\napple\nbanana\ncherry\n");
}

TEST(SortTest, EmptyFile) {
    TempDir tmp;
    auto f = tmp.write_file("empty.txt", "");
    char a0[] = "sort", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return sort_main(2, argv); });
    EXPECT_EQ(out, "");
}
