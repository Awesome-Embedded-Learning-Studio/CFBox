#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_REV
#include <cfbox/applets.hpp>

TEST(RevTest, HelpAndVersion) {
    char a0[] = "rev";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = rev_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("rev"), std::string::npos);
}

TEST(RevTest, NonExistentFile) {
    char a0[] = "rev";
    char a1[] = "/nonexistent_file_xyz";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = rev_main(2, argv);
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(rc, 1);
}
#endif
