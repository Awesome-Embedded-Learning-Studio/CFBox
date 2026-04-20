#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_LS

using namespace cfbox::test;

TEST(LsTest, ListDirectory) {
    TempDir tmp;
    tmp.write_file("apple.txt", "");
    tmp.write_file("banana.txt", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", dir.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return ls_main(2, argv); });
    EXPECT_NE(out.find("apple.txt"), std::string::npos);
    EXPECT_NE(out.find("banana.txt"), std::string::npos);
}

TEST(LsTest, ListAllWithHidden) {
    TempDir tmp;
    tmp.write_file("visible.txt", "");
    tmp.write_file(".hidden", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[] = "-a", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("visible.txt"), std::string::npos);
    EXPECT_NE(out.find(".hidden"), std::string::npos);
}

TEST(LsTest, ListHiddenNotShownByDefault) {
    TempDir tmp;
    tmp.write_file("visible.txt", "");
    tmp.write_file(".hidden", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", dir.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return ls_main(2, argv); });
    EXPECT_NE(out.find("visible.txt"), std::string::npos);
    EXPECT_EQ(out.find(".hidden"), std::string::npos);
}

TEST(LsTest, LongFormatContainsPermissions) {
    TempDir tmp;
    tmp.write_file("file.txt", "data");
    auto f = (tmp.path / "file.txt").string();
    char a0[] = "ls", a1[] = "-l", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("-rw"), std::string::npos);
    EXPECT_NE(out.find("file.txt"), std::string::npos);
}

TEST(LsTest, NonexistentPath) {
    char a0[] = "ls", a1[] = "/no/such/path";
    char* argv[] = {a0, a1};
    EXPECT_NE(ls_main(2, argv), 0);
}

TEST(LsTest, SingleFileNoDirectory) {
    TempDir tmp;
    tmp.write_file("only.txt", "data");
    auto f = (tmp.path / "only.txt").string();
    char a0[] = "ls", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return ls_main(2, argv); });
    EXPECT_EQ(out, "only.txt\n");
}

#endif // CFBOX_ENABLE_LS
