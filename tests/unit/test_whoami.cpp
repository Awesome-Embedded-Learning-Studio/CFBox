#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_WHOAMI

using namespace cfbox::test;

TEST(WhoamiTest, PrintsName) {
    char a0[] = "whoami";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return whoami_main(1, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find('\n'), std::string::npos);
}

#endif // CFBOX_ENABLE_WHOAMI
