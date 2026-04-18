#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(RmTest, RemoveSingleFile) {
    TempDir tmp;
    auto f = tmp.write_file("to_delete.txt", "data");
    char a0[] = "rm", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    EXPECT_EQ(rm_main(2, argv), 0);
    EXPECT_FALSE(std::filesystem::exists(f));
}

TEST(RmTest, RemoveNonexistentFails) {
    TempDir tmp;
    auto f = (tmp.path / "no_such_file").string();
    char a0[] = "rm", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    EXPECT_NE(rm_main(2, argv), 0);
}

TEST(RmTest, ForceNonexistentOk) {
    TempDir tmp;
    auto f = (tmp.path / "no_such_file").string();
    char a0[] = "rm", a1[] = "-f", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(rm_main(3, argv), 0);
}

TEST(RmTest, RemoveDirectoryWithoutRecursiveFails) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "subdir");
    auto subdir = (tmp.path / "subdir").string();
    char a0[] = "rm", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", subdir.c_str());
    char* argv[] = {a0, a1};
    EXPECT_NE(rm_main(2, argv), 0);
}

TEST(RmTest, RemoveRecursive) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "subdir");
    tmp.write_file("subdir/file.txt", "data");
    auto subdir = (tmp.path / "subdir").string();
    char a0[] = "rm", a1[] = "-r", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", subdir.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(rm_main(3, argv), 0);
    EXPECT_FALSE(std::filesystem::exists(tmp.path / "subdir"));
}

TEST(RmTest, MissingOperand) {
    char a0[] = "rm";
    char* argv[] = {a0};
    EXPECT_NE(rm_main(1, argv), 0);
}
