#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_TOUCH
using namespace cfbox::test;
TEST(TouchTest, CreatesFile) {
    TempDir tmp;
    auto filepath = (tmp.path / "touched").string();
    char a0[] = "touch", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", filepath.c_str());
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(touch_main(2, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(filepath));
}
#endif
