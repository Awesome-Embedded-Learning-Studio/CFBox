#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_REALPATH
using namespace cfbox::test;
TEST(RealpathTest, ResolvesPath) {
    TempDir tmp;
    auto filepath = tmp.write_file("test.txt", "data");
    char a0[] = "realpath", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", filepath.c_str());
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return realpath_main(2, argv); });
    ASSERT_FALSE(out.empty());
}
#endif
