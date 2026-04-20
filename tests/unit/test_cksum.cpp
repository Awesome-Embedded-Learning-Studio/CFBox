#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_CKSUM

using namespace cfbox::test;

TEST(CksumTest, KnownFile) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\n");
    char a0[] = "cksum", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return cksum_main(2, argv); });
    EXPECT_FALSE(out.empty());
}

TEST(CksumTest, OutputFormat) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "test content\n");
    char a0[] = "cksum", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return cksum_main(2, argv); });
    // Output format: <checksum> <size> <filename>
    EXPECT_NE(out.find(" "), std::string::npos);
    // Should contain digits
    bool has_digit = false;
    for (char c : out) { if (c >= '0' && c <= '9') { has_digit = true; break; } }
    EXPECT_TRUE(has_digit);
}

#endif // CFBOX_ENABLE_CKSUM
