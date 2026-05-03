#include <gtest/gtest.h>
#include <cfbox/applets.hpp>

#if CFBOX_ENABLE_KILL
TEST(KillTest, ListSignals) {
    char a0[] = "kill";
    char a1[] = "-l";
    char* argv[] = {a0, a1, nullptr};
    testing::internal::CaptureStdout();
    int rc = kill_main(2, argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("TERM"), std::string::npos);
    EXPECT_NE(output.find("KILL"), std::string::npos);
}

TEST(KillTest, NoPidError) {
    char a0[] = "kill";
    char* argv[] = {a0, nullptr};
    testing::internal::CaptureStderr();
    int rc = kill_main(1, argv);
    std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(rc, 0);
    EXPECT_NE(err.find("no PID"), std::string::npos);
}
#endif
