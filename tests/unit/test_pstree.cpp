#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_PSTREE
#include <cfbox/applets.hpp>

TEST(PstreeTest, HelpAndVersion) {
    char a0[] = "pstree";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = pstree_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("pstree"), std::string::npos);
}

TEST(PstreeTest, Runs) {
    char a0[] = "pstree";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    int rc = pstree_main(1, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(out.empty());
}

TEST(PstreeTest, ShowPids) {
    char a0[] = "pstree";
    char a1[] = "-p";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = pstree_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    // Should contain PID in parentheses
    EXPECT_NE(out.find('('), std::string::npos);
}
#endif
