#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_LN
using namespace cfbox::test;
TEST(LnTest, HardLink) {
    TempDir tmp;
    auto src = tmp.write_file("src.txt", "hello");
    auto dst = (tmp.path / "dst.txt").string();
    char a0[] = "ln", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", src.c_str());
    std::snprintf(a2, sizeof(a2), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(ln_main(3, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(dst));
}
#endif
