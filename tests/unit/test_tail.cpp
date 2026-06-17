#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_TAIL

using namespace cfbox::test;

TEST(TailTest, DefaultTenLines) {
    TempDir tmp;
    tmp.write_file("data.txt", "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "tail", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return tail_main(2, argv); });
    // Should have last 10 lines (3..12)
    auto count = std::count(out.begin(), out.end(), '\n');
    EXPECT_EQ(count, 10);
    EXPECT_NE(out.find("3\n"), std::string::npos);
}

TEST(TailTest, NLines) {
    TempDir tmp;
    tmp.write_file("data.txt", "a\nb\nc\nd\ne\n");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "tail", a1[] = "-n", a2[] = "2", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return tail_main(4, argv); });
    EXPECT_EQ(out, "d\ne\n");
}

TEST(TailTest, CBytes) {
    TempDir tmp;
    tmp.write_file("data.txt", "abcdef");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "tail", a1[] = "-c", a2[] = "3", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return tail_main(4, argv); });
    EXPECT_EQ(out, "def");
}

TEST(TailTest, PlusNFromStart) {
    TempDir tmp;
    tmp.write_file("data.txt", "a\nb\nc\nd\ne\n");
    auto f = (tmp.path / "data.txt").string();
    char a0[] = "tail", a1[] = "-n", a2[] = "+3", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return tail_main(4, argv); });
    EXPECT_EQ(out, "c\nd\ne\n");
}

TEST(TailTest, NonexistentFile) {
    char a0[] = "tail", a1[] = "/no/such/file.txt";
    char* argv[] = {a0, a1};
    EXPECT_NE(tail_main(2, argv), 0);
}

// Follow-mode option handling. The follow loop itself blocks (it waits on
// appended data / SIGINT), so it is exercised by the integration script; here
// we cover the argument-parsing edges that exit before the loop starts.
TEST(TailFollow, NoFilesErrors) {
    char a0[] = "tail", a1[] = "-f";
    char* argv[] = {a0, a1};
    EXPECT_NE(tail_main(2, argv), 0);
}

TEST(TailFollow, BadIntervalErrors) {
    char a0[] = "tail", a1[] = "-s", a2[] = "abc", a3[] = "-f";
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_NE(tail_main(4, argv), 0);
}

#endif // CFBOX_ENABLE_TAIL
