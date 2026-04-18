#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(MkdirTest, CreateSingleDir) {
    TempDir tmp;
    auto newdir = (tmp.path / "newdir").string();
    char a0[] = "mkdir", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", newdir.c_str());
    char* argv[] = {a0, a1};
    EXPECT_EQ(mkdir_main(2, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "newdir"));
    EXPECT_TRUE(std::filesystem::is_directory(tmp.path / "newdir"));
}

TEST(MkdirTest, RecursiveCreate) {
    TempDir tmp;
    auto deep = (tmp.path / "a" / "b" / "c").string();
    char a0[] = "mkdir", a1[] = "-p", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", deep.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(mkdir_main(3, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "a" / "b" / "c"));
}

TEST(MkdirTest, ExistingDirFails) {
    TempDir tmp;
    auto existing = tmp.path.string();
    char a0[] = "mkdir", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", existing.c_str());
    char* argv[] = {a0, a1};
    EXPECT_NE(mkdir_main(2, argv), 0);
}

TEST(MkdirTest, MissingOperand) {
    char a0[] = "mkdir";
    char* argv[] = {a0};
    EXPECT_NE(mkdir_main(1, argv), 0);
}

TEST(MkdirTest, MultipleDirs) {
    TempDir tmp;
    auto d1 = (tmp.path / "dir1").string();
    auto d2 = (tmp.path / "dir2").string();
    char a0[] = "mkdir", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", d1.c_str());
    std::snprintf(a2, sizeof(a2), "%s", d2.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(mkdir_main(3, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dir1"));
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dir2"));
}
