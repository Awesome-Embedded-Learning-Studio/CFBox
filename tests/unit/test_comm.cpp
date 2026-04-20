#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_COMM

using namespace cfbox::test;

TEST(CommTest, TwoSortedFiles) {
    TempDir tmp;
    auto f1 = tmp.write_file("a.txt", "apple\nbanana\ncherry\n");
    auto f2 = tmp.write_file("b.txt", "banana\ncherry\ndate\n");
    char a0[] = "comm", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", f1.c_str());
    std::snprintf(a2, sizeof(a2), "%s", f2.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return comm_main(3, argv); });
    EXPECT_NE(out.find("apple"), std::string::npos);
    EXPECT_NE(out.find("date"), std::string::npos);
    EXPECT_NE(out.find("banana"), std::string::npos);
    EXPECT_NE(out.find("cherry"), std::string::npos);
}

#endif // CFBOX_ENABLE_COMM
