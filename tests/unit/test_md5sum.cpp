#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_MD5SUM

using namespace cfbox::test;

TEST(Md5sumTest, OutputFormat) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\n");
    char a0[] = "md5sum", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return md5sum_main(2, argv); });
    EXPECT_FALSE(out.empty());
    // Output should have 32 hex characters followed by "  " and filename
    // Find the two-space separator
    auto pos = out.find("  ");
    ASSERT_NE(pos, std::string::npos);
    EXPECT_EQ(pos, 32u);
    // Verify all 32 chars are hex
    for (std::size_t i = 0; i < 32u; ++i) {
        bool hex = (out[i] >= '0' && out[i] <= '9') ||
                   (out[i] >= 'a' && out[i] <= 'f');
        EXPECT_TRUE(hex) << "char at " << i << " is not hex: " << out[i];
    }
}

#endif // CFBOX_ENABLE_MD5SUM
