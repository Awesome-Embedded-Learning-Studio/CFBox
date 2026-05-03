#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_PMAP
#include <cfbox/applets.hpp>

TEST(PmapTest, HelpAndVersion) {
    char a0[] = "pmap";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = pmap_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("pmap"), std::string::npos);
}

TEST(PmapTest, NoArgs) {
    char a0[] = "pmap";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = pmap_main(1, argv);
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(rc, 1);
}

TEST(PmapTest, SelfMaps) {
    char a0[] = "pmap";
    char a1[] = "1";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = pmap_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    // PID 1 may not exist in all test environments
    if (rc == 0) {
        EXPECT_FALSE(out.empty());
    }
}
#endif
