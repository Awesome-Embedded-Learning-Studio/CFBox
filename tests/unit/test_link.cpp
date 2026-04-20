#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_LINK

using namespace cfbox::test;

TEST(LinkTest, CreatesHardLink) {
    TempDir tmp;
    auto file = tmp.write_file("original.txt", "content");
    auto link_path = (std::filesystem::path{tmp.path} / "link.txt").string();

    char a0[] = "link";
    char a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", file.c_str());
    std::snprintf(a2, sizeof(a2), "%s", link_path.c_str());
    char* argv[] = {a0, a1, a2, nullptr};

    EXPECT_EQ(link_main(3, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(link_path));

    // Same inode (hard link)
    auto stat1 = std::filesystem::hard_link_count(file);
    EXPECT_GE(stat1, 2u);
}

TEST(LinkTest, MissingOperand) {
    char a0[] = "link", a1[] = "onlyone";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(link_main(2, argv), 1);
}

#endif // CFBOX_ENABLE_LINK
