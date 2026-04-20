#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_TTY

using namespace cfbox::test;

TEST(TtyTest, HelpReturnsZero) {
    char a0[] = "tty", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return tty_main(2, argv); });
    EXPECT_NE(out.find("tty"), std::string::npos);
    EXPECT_EQ(tty_main(2, argv), 0);
}

#endif // CFBOX_ENABLE_TTY
