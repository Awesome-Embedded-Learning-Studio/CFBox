#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_RMDIR
using namespace cfbox::test;

TEST(RmdirTest, RemovesEmptyDir) {
    TempDir tmp;
    auto subdir = (tmp.path / "sub").string();
    std::filesystem::create_directory(subdir);
    ASSERT_TRUE(std::filesystem::exists(subdir));
    char a0[] = "rmdir", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", subdir.c_str());
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(rmdir_main(2, argv), 0);
    EXPECT_FALSE(std::filesystem::exists(subdir));
}

TEST(RmdirTest, NonEmptyFails) {
    TempDir tmp;
    auto subdir = (tmp.path / "sub").string();
    std::filesystem::create_directory(subdir);
    tmp.write_file("sub/file.txt", "data");
    char a0[] = "rmdir", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", subdir.c_str());
    char* argv[] = {a0, a1, nullptr};
    EXPECT_NE(rmdir_main(2, argv), 0);
}
#endif
