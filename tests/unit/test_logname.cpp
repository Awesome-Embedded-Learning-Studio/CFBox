#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_LOGNAME

using namespace cfbox::test;

TEST(LognameTest, PrintsName) {
    char a0[] = "logname";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return logname_main(1, argv); });
    // May succeed (getlogin or env var) or fail (no terminal in CI)
    // Just check it doesn't crash
    if (!out.empty()) {
        EXPECT_NE(out.find('\n'), std::string::npos);
    }
}

#endif // CFBOX_ENABLE_LOGNAME
