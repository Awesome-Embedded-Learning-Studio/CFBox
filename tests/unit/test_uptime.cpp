#include <gtest/gtest.h>
#include <cfbox/applets.hpp>

#if CFBOX_ENABLE_UPTIME
TEST(UptimeTest, RunsAndOutputs) {
    char a0[] = "uptime";
    char* argv[] = {a0, nullptr};
    testing::internal::CaptureStdout();
    int rc = uptime_main(1, argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("up"), std::string::npos);
    EXPECT_NE(output.find("load average"), std::string::npos);
}
#endif
