#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(MvTest, MoveSingleFile) {
    TempDir tmp;
    auto src = tmp.write_file("src.txt", "content");
    auto dst = (tmp.path / "dst.txt").string();
    char a0[] = "mv", a1[] = "-f", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", src.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(mv_main(4, argv), 0);
    EXPECT_FALSE(std::filesystem::exists(src));
    auto content = cfbox::io::read_all(dst);
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content.value(), "content");
}

TEST(MvTest, MoveIntoDirectory) {
    TempDir tmp;
    auto src = tmp.write_file("file.txt", "data");
    std::filesystem::create_directory(tmp.path / "dir");
    auto dir = (tmp.path / "dir").string();
    char a0[] = "mv", a1[] = "-f", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", src.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(mv_main(4, argv), 0);
    EXPECT_FALSE(std::filesystem::exists(src));
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dir" / "file.txt"));
}

TEST(MvTest, SourceNotExist) {
    TempDir tmp;
    auto fake = (tmp.path / "nonexistent").string();
    auto dst = (tmp.path / "dst").string();
    char a0[] = "mv", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", fake.c_str());
    std::snprintf(a2, sizeof(a2), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_NE(mv_main(3, argv), 0);
}

TEST(MvTest, MissingOperands) {
    char a0[] = "mv";
    char* argv[] = {a0};
    EXPECT_NE(mv_main(1, argv), 0);
}
