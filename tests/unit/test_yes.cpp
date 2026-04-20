#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_YES

TEST(YesTest, Help) {
    char a0[] = "yes", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return yes_main(2, argv); });
    EXPECT_NE(out.find("yes"), std::string::npos);
    EXPECT_EQ(yes_main(2, argv), 0);
}

TEST(YesTest, Version) {
    char a0[] = "yes", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return yes_main(2, argv); });
    EXPECT_NE(out.find("cfbox"), std::string::npos);
}

#endif // CFBOX_ENABLE_YES
