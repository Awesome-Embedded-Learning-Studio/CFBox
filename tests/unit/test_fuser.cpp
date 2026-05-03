#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_FUSER
#include <cfbox/applets.hpp>

TEST(FuserTest, HelpAndVersion) {
    char a0[] = "fuser";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = fuser_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("fuser"), std::string::npos);
}

TEST(FuserTest, NoArgs) {
    char a0[] = "fuser";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = fuser_main(1, argv);
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(rc, 1);
}

TEST(FuserTest, NonExistentFile) {
    char a0[] = "fuser";
    char a1[] = "/nonexistent_file_xyz";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = fuser_main(2, argv);
    // Should fail to stat or find no processes
    EXPECT_NE(rc, 0);
}
#endif
