#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_HEXDUMP
#include <cfbox/applets.hpp>

TEST(HexdumpTest, HelpAndVersion) {
    char a0[] = "hexdump";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = hexdump_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("hexdump"), std::string::npos);
}

TEST(HexdumpTest, CanonicalStdin) {
    char a0[] = "hexdump";
    char a1[] = "-C";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = hexdump_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    // Empty stdin produces no output
    EXPECT_TRUE(out.empty());
}
#endif
