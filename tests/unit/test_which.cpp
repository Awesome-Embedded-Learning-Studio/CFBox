#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_WHICH

TEST(WhichTest, FindsExistingCommand) {
    char a0[] = "which", a1[] = "ls";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return which_main(2, argv); });
    EXPECT_EQ(which_main(2, argv), 0);
    EXPECT_NE(out.find("/ls"), std::string::npos);
}

TEST(WhichTest, FailsForNonexistent) {
    char a0[] = "which", a1[] = "nonexistent_command_xyz_12345";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_NE(which_main(2, argv), 0);
}

TEST(WhichTest, MissingArg) {
    char a0[] = "which";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(which_main(1, argv), 2);
}

TEST(WhichTest, Help) {
    char a0[] = "which", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return which_main(2, argv); });
    EXPECT_NE(out.find("which"), std::string::npos);
}

#endif // CFBOX_ENABLE_WHICH
