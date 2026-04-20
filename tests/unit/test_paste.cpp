#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_PASTE

using namespace cfbox::test;

TEST(PasteTest, SingleFile) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\nb\nc\n");
    char a0[] = "paste", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return paste_main(2, argv); });
    EXPECT_EQ(out, "a\nb\nc\n");
}

#endif // CFBOX_ENABLE_PASTE
