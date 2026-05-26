#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_HEAD

using namespace cfbox::test;

TEST(HeadTest, DefaultTenLines) {
    TempDir tmp;
    tmp.write_file("data.txt", "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "head", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return head_main(2, argv); });
    // Should have 10 lines, not 12
    auto count = std::count(out.begin(), out.end(), '\n');
    EXPECT_EQ(count, 10);
}

TEST(HeadTest, NLines) {
    TempDir tmp;
    tmp.write_file("data.txt", "a\nb\nc\nd\ne\n");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "head", a1[] = "-n", a2[] = "3", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return head_main(4, argv); });
    EXPECT_EQ(out, "a\nb\nc\n");
}

TEST(HeadTest, CBytes) {
    TempDir tmp;
    tmp.write_file("data.txt", "abcdef");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "head", a1[] = "-c", a2[] = "3", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return head_main(4, argv); });
    EXPECT_EQ(out, "abc");
}

TEST(HeadTest, FileShorterThanN) {
    TempDir tmp;
    tmp.write_file("short.txt", "only\n");
    auto f = (tmp.path / "short.txt").string();
    char a0[] = "head", a1[] = "-n", a2[] = "10", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return head_main(4, argv); });
    EXPECT_EQ(out, "only\n");
}

TEST(HeadTest, NonexistentFile) {
    char a0[] = "head", a1[] = "/no/such/file.txt";
    char* argv[] = {a0, a1};
    EXPECT_NE(head_main(2, argv), 0);
}

#endif // CFBOX_ENABLE_HEAD
