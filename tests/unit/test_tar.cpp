#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_TAR

using namespace cfbox::test;

TEST(TarTest, HelpOutput) {
    char a0[] = "tar", a1[] = "--help";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return tar_main(2, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find("archive"), std::string::npos);
}

TEST(TarTest, CreateListExtract) {
    TempDir tmp;
    tmp.write_file("hello.txt", "hello world");
    auto archive = (tmp.path / "test.tar").string();

    auto saved = std::filesystem::current_path();
    std::filesystem::current_path(tmp.path);

    // Create
    char a0[] = "tar", a1[] = "-cf", a2[] = "test.tar", a3[] = "hello.txt";
    char* argv1[] = {a0, a1, a2, a3};
    int rc = tar_main(4, argv1);
    EXPECT_EQ(rc, 0);

    // List
    char b0[] = "tar", b1[] = "-tf", b2[] = "test.tar";
    char* argv2[] = {b0, b1, b2};
    auto listing = capture_stdout([&]{ return tar_main(3, argv2); });
    EXPECT_NE(listing.find("hello.txt"), std::string::npos);

    // Extract into subdirectory
    std::filesystem::create_directory(tmp.path / "out");
    std::filesystem::current_path(tmp.path / "out");
    char c0[] = "tar", c1[] = "-xf", c2[256];
    std::snprintf(c2, sizeof(c2), "%s", archive.c_str());
    char* argv3[] = {c0, c1, c2};
    rc = tar_main(3, argv3);
    EXPECT_EQ(rc, 0);

    std::filesystem::current_path(saved);
    auto outfile = tmp.path / "out" / "hello.txt";
    ASSERT_TRUE(std::filesystem::exists(outfile));
    auto content = cfbox::io::read_all(outfile.string());
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(*content, "hello world");
}

#endif // CFBOX_ENABLE_TAR
