#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_FALSE

TEST(FalseTest, ReturnsOne) {
    char a0[] = "false";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(false_main(1, argv), 1);
}

TEST(FalseTest, IgnoresArgs) {
    char a0[] = "false", a1[] = "anything";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(false_main(2, argv), 1);
}

TEST(FalseTest, HelpReturnsZero) {
    char a0[] = "false", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(false_main(2, argv), 0);
}

TEST(FalseTest, VersionReturnsZero) {
    char a0[] = "false", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return false_main(2, argv); });
    EXPECT_NE(out.find("cfbox"), std::string::npos);
    EXPECT_EQ(false_main(2, argv), 0);
}

#endif // CFBOX_ENABLE_FALSE
