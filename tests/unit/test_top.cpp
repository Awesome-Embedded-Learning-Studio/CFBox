#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_TOP
#include <cfbox/applets.hpp>

TEST(TopTest, HelpAndVersion) {
    char a0[] = "top";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = top_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("top"), std::string::npos);
}

TEST(TopTest, BatchMode) {
    char a0[] = "top";
    char a1[] = "-b";
    char a2[] = "-n";
    char a3[] = "1";
    char* argv[] = {a0, a1, a2, a3, nullptr};

    testing::internal::CaptureStdout();
    int rc = top_main(4, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("PID"), std::string::npos);
}
#endif
