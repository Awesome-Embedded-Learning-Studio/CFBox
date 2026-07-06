#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>

#include "test_capture.hpp"

#include <gtest/gtest.h>

#if CFBOX_ENABLE_IP

using namespace cfbox::test;

TEST(IpTest, AddrShowListsLoopback) {
    char a0[] = "ip", a1[] = "addr", a2[] = "show";
    char* argv[] = {a0, a1, a2, nullptr};
    auto out = capture_stdout([&] { return ip_main(3, argv); });
    EXPECT_NE(out.find("lo"), std::string::npos);
    EXPECT_NE(out.find("LOOPBACK"), std::string::npos);
    EXPECT_NE(out.find("127.0.0.1"), std::string::npos);
}

TEST(IpTest, AddrAbbrevWorks) {
    char a0[] = "ip", a1[] = "a";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return ip_main(2, argv); });
    EXPECT_NE(out.find("link/"), std::string::npos);
}

TEST(IpTest, NoSubcommandExits2) {
    char a0[] = "ip";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(ip_main(1, argv), 2);
}

#endif // CFBOX_ENABLE_IP
