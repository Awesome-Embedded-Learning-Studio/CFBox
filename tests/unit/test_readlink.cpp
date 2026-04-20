#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#if CFBOX_ENABLE_READLINK
using namespace cfbox::test;
TEST(ReadlinkTest, ReadsSymlink) {
    TempDir tmp;
    auto target = tmp.write_file("target.txt", "data");
    auto link = (tmp.path / "link").string();
    std::filesystem::create_symlink(target, link);
    char a0[] = "readlink", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", link.c_str());
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return readlink_main(2, argv); });
    EXPECT_EQ(out.substr(0, out.size() - 1), target);
}
#endif
