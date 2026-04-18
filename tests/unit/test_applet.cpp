#include <cfbox/applet.hpp>
#include <gtest/gtest.h>

using namespace cfbox::applet;

// ── fake applet functions ─────────────────────────────────────

static int fake_echo([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) { return 0; }
static int fake_cat([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) { return 1; }

// ── test registry ─────────────────────────────────────────────

constexpr auto TEST_REGISTRY = std::to_array<AppEntry>({
    {"echo", fake_echo, "display a line of text"},
    {"cat",  fake_cat,  "concatenate files"},
});

// ── AppEntry fields ───────────────────────────────────────────

TEST(AppletTest, EntryFields) {
    const auto& entry = TEST_REGISTRY[0];
    EXPECT_EQ(entry.app_name, "echo");
    EXPECT_EQ(entry.help, "display a line of text");
    EXPECT_EQ(entry.app_func, fake_echo);
}

// ── find_applet ───────────────────────────────────────────────

TEST(AppletTest, FindExisting) {
    const auto* found = find_applet("echo", TEST_REGISTRY);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->app_name, "echo");
    EXPECT_EQ(found->app_func, fake_echo);
}

TEST(AppletTest, FindLast) {
    const auto* found = find_applet("cat", TEST_REGISTRY);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->app_name, "cat");
    EXPECT_EQ(found->app_func, fake_cat);
}

TEST(AppletTest, FindMissing) {
    const auto* found = find_applet("nonexistent", TEST_REGISTRY);
    EXPECT_EQ(found, nullptr);
}

TEST(AppletTest, EmptyName) {
    const auto* found = find_applet("", TEST_REGISTRY);
    EXPECT_EQ(found, nullptr);
}
