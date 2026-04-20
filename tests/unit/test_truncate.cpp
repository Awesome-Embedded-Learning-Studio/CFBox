#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_TRUNCATE
using namespace cfbox::test;
TEST(TruncateTest, SetsSize) {
    TempDir tmp;
    auto filepath = tmp.write_file("test.txt", "hello world");
    char a0[] = "truncate", a1[] = "-s", a2[] = "5", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", filepath.c_str());
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(truncate_main(4, argv), 0);
    EXPECT_EQ(std::filesystem::file_size(filepath), 5u);
}
#endif
