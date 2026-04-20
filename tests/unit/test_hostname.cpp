#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_HOSTNAME

using namespace cfbox::test;

TEST(HostnameTest, PrintsName) {
    char a0[] = "hostname";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return hostname_main(1, argv); });
    EXPECT_FALSE(out.empty());
}

TEST(HostnameTest, ShortFlag) {
    char a0[] = "hostname", a1[] = "-s";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return hostname_main(2, argv); });
    EXPECT_FALSE(out.empty());
    // Short name should not contain dots
    EXPECT_EQ(out.find('.'), std::string::npos);
}

#endif // CFBOX_ENABLE_HOSTNAME
