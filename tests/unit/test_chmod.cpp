#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"
#include <filesystem>

#if CFBOX_ENABLE_CHMOD

using namespace cfbox::test;

TEST(ChmodTest, OctalMode) {
    TempDir tmp;
    auto f = tmp.write_file("test.txt", "data");
    char a0[] = "chmod", a1[] = "755", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(chmod_main(3, argv), 0);

    auto st = std::filesystem::status(f);
    auto perms = st.permissions();
    EXPECT_TRUE((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
    EXPECT_TRUE((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
    EXPECT_TRUE((perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none);
    EXPECT_TRUE((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none);
}

TEST(ChmodTest, SymbolicMode) {
    TempDir tmp;
    auto f = tmp.write_file("test.txt", "data");
    // First set to 644
    char a0[] = "chmod", a1[] = "644", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv1[] = {a0, a1, a2, nullptr};
    chmod_main(3, argv1);

    // Now add execute: u+x
    char b1[] = "u+x";
    char* argv2[] = {a0, b1, a2, nullptr};
    EXPECT_EQ(chmod_main(3, argv2), 0);

    auto st = std::filesystem::status(f);
    EXPECT_TRUE((st.permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
}

TEST(ChmodTest, MissingOperand) {
    char a0[] = "chmod";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(chmod_main(1, argv), 2);
}

TEST(ChmodTest, Help) {
    char a0[] = "chmod", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return chmod_main(2, argv); });
    EXPECT_NE(out.find("chmod"), std::string::npos);
}

#endif // CFBOX_ENABLE_CHMOD
