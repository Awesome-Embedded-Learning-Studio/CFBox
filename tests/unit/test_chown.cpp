#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_CHOWN

TEST(ChownTest, MissingOperand) {
    char a0[] = "chown";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(chown_main(1, argv), 2);
}

TEST(ChownTest, NonexistentFile) {
    char a0[] = "chown", a1[] = "root", a2[] = "/nonexistent_xyz_12345";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_NE(chown_main(3, argv), 0);
}

TEST(ChownTest, Help) {
    char a0[] = "chown", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return chown_main(2, argv); });
    EXPECT_NE(out.find("chown"), std::string::npos);
}

#endif // CFBOX_ENABLE_CHOWN
