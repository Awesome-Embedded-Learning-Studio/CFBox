#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_MKTEMP
using namespace cfbox::test;
TEST(MktempTest, CreatesFile) {
    char a0[] = "mktemp";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return mktemp_main(1, argv); });
    ASSERT_FALSE(out.empty());
    auto path = out.substr(0, out.size() - 1);
    EXPECT_TRUE(std::filesystem::exists(path));
    std::filesystem::remove(path);
}
#endif
