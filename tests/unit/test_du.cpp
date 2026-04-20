#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_DU
using namespace cfbox::test;
TEST(DuTest, PrintsSize) {
    TempDir tmp;
    tmp.write_file("test.txt", "hello world");
    char a0[] = "du", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return du_main(2, argv); });
    EXPECT_FALSE(out.empty());
}
#endif
