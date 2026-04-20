#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_ID

using namespace cfbox::test;

TEST(IdTest, DefaultFormat) {
    char a0[] = "id";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return id_main(1, argv); });
    EXPECT_NE(out.find("uid="), std::string::npos);
    EXPECT_NE(out.find("gid="), std::string::npos);
}

TEST(IdTest, UserFlag) {
    char a0[] = "id", a1[] = "-u";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return id_main(2, argv); });
    int val = std::stoi(out);
    EXPECT_GE(val, 0);
}

TEST(IdTest, GroupFlag) {
    char a0[] = "id", a1[] = "-g";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return id_main(2, argv); });
    int val = std::stoi(out);
    EXPECT_GE(val, 0);
}

TEST(IdTest, UserWithName) {
    char a0[] = "id", a1[] = "-u", a2[] = "-n";
    char* argv[] = {a0, a1, a2, nullptr};
    auto out = capture_stdout([&] { return id_main(3, argv); });
    EXPECT_FALSE(out.empty());
    // Should print a username (non-empty, ends with newline)
    EXPECT_NE(out.find('\n'), std::string::npos);
}

#endif // CFBOX_ENABLE_ID
