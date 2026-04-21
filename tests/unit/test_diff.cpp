#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_DIFF

using namespace cfbox::test;

TEST(DiffTest, HelpOutput) {
    char a0[] = "diff", a1[] = "--help";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return diff_main(2, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find("compare"), std::string::npos);
}

TEST(DiffTest, IdenticalFiles) {
    TempDir tmp;
    auto f1 = tmp.write_file("a.txt", "hello\nworld\n");
    auto f2 = tmp.write_file("b.txt", "hello\nworld\n");
    char a0[] = "diff", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", f1.c_str());
    std::snprintf(a2, sizeof(a2), "%s", f2.c_str());
    char* argv[] = {a0, a1, a2};
    int rc = diff_main(3, argv);
    EXPECT_EQ(rc, 0);
}

TEST(DiffTest, DifferentFiles) {
    TempDir tmp;
    auto f1 = tmp.write_file("a.txt", "hello\nworld\n");
    auto f2 = tmp.write_file("b.txt", "hello\nearth\n");
    char a0[] = "diff", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", f1.c_str());
    std::snprintf(a2, sizeof(a2), "%s", f2.c_str());
    char* argv[] = {a0, a1, a2};
    int rc = diff_main(3, argv);
    EXPECT_EQ(rc, 1);
}

#endif // CFBOX_ENABLE_DIFF
