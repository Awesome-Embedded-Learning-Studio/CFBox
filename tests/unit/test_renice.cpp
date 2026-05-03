#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_RENICE
#include <cfbox/applets.hpp>

TEST(ReniceTest, HelpAndVersion) {
    char a0[] = "renice";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = renice_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("renice"), std::string::npos);
}

TEST(ReniceTest, NoArgs) {
    char a0[] = "renice";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = renice_main(1, argv);
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(rc, 1);
}
#endif
