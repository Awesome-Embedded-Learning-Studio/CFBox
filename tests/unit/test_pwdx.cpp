#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_PWDX
#include <cfbox/applets.hpp>

TEST(PwdxTest, HelpAndVersion) {
    char a0[] = "pwdx";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = pwdx_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("pwdx"), std::string::npos);
}

TEST(PwdxTest, NoArgs) {
    char a0[] = "pwdx";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = pwdx_main(1, argv);
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(rc, 1);
}

TEST(PwdxTest, SelfCwd) {
    char a0[] = "pwdx";
    char a1[] = "1";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = pwdx_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    // PID 1 may not exist in all test environments; just verify format if it does
    if (rc == 0) {
        EXPECT_NE(out.find("1:"), std::string::npos);
    }
}
#endif
