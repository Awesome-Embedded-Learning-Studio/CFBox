#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_HOSTID
using namespace cfbox::test;

TEST(HostidTest, PrintsHexId) {
    char a0[] = "hostid";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return hostid_main(1, argv); });
    ASSERT_GE(out.size(), 8u);
}
#endif
