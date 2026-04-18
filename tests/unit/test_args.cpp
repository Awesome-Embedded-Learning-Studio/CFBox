#include <cfbox/args.hpp>
#include <gtest/gtest.h>

using namespace cfbox::args;

// ── no args ───────────────────────────────────────────────────

TEST(ArgsTest, NoArgs) {
    char a0[] = "prog";
    char* argv[] = {a0};
    auto r = parse(1, argv, {});
    EXPECT_TRUE(r.positional().empty());
    EXPECT_FALSE(r.has('n'));
}

// ── positional only ───────────────────────────────────────────

TEST(ArgsTest, PositionalOnly) {
    char a0[] = "prog", a1[] = "hello", a2[] = "world";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {});
    ASSERT_EQ(r.positional().size(), 2u);
    EXPECT_EQ(r.positional()[0], "hello");
    EXPECT_EQ(r.positional()[1], "world");
}

// ── bool flag ─────────────────────────────────────────────────

TEST(ArgsTest, BoolFlag) {
    char a0[] = "prog", a1[] = "-n";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'n', false}});
    EXPECT_TRUE(r.has('n'));
    EXPECT_FALSE(r.get('n').has_value());
    EXPECT_TRUE(r.positional().empty());
}

// ── value flag attached (-n5) ────────────────────────────────

TEST(ArgsTest, ValueFlagAttached) {
    char a0[] = "prog", a1[] = "-n5";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'n', true}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_TRUE(r.get('n').has_value());
    EXPECT_EQ(r.get('n').value(), "5");
}

// ── value flag separate (-n 10) ──────────────────────────────

TEST(ArgsTest, ValueFlagSeparate) {
    char a0[] = "prog", a1[] = "-n", a2[] = "10";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'n', true}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_TRUE(r.get('n').has_value());
    EXPECT_EQ(r.get('n').value(), "10");
}

// ── combined bool flags (-ne) ────────────────────────────────

TEST(ArgsTest, CombinedBoolFlags) {
    char a0[] = "prog", a1[] = "-ne";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'n', false}, OptSpec{'e', false}});
    EXPECT_TRUE(r.has('n'));
    EXPECT_TRUE(r.has('e'));
}

// ── -- stops parsing ─────────────────────────────────────────

TEST(ArgsTest, DoubleDashStopsParsing) {
    char a0[] = "prog", a1[] = "--", a2[] = "-n";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'n', false}});
    EXPECT_FALSE(r.has('n'));
    ASSERT_EQ(r.positional().size(), 1u);
    EXPECT_EQ(r.positional()[0], "-n");
}

// ── bare "-" is positional (stdin) ───────────────────────────

TEST(ArgsTest, DashIsPositional) {
    char a0[] = "prog", a1[] = "-";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {});
    ASSERT_EQ(r.positional().size(), 1u);
    EXPECT_EQ(r.positional()[0], "-");
}

// ── mixed flags and positional ────────────────────────────────

TEST(ArgsTest, MixedFlagsAndPositional) {
    char a0[] = "prog", a1[] = "-n", a2[] = "hello", a3[] = "world";
    char* argv[] = {a0, a1, a2, a3};
    auto r = parse(4, argv, {OptSpec{'n', false}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_EQ(r.positional().size(), 2u);
    EXPECT_EQ(r.positional()[0], "hello");
    EXPECT_EQ(r.positional()[1], "world");
}

// ── value flag with negative number (-n -5) ──────────────────

TEST(ArgsTest, ValueFlagNegativeNumber) {
    char a0[] = "prog", a1[] = "-n", a2[] = "-5";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'n', true}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_TRUE(r.get('n').has_value());
    EXPECT_EQ(r.get('n').value(), "-5");
}

// ── unknown flag treated as bool ──────────────────────────────

TEST(ArgsTest, UnknownFlagTreatedAsBool) {
    char a0[] = "prog", a1[] = "-x";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {});
    EXPECT_TRUE(r.has('x'));
}

// ── value flag at end with no value ──────────────────────────

TEST(ArgsTest, ValueFlagMissingValue) {
    char a0[] = "prog", a1[] = "-n";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'n', true}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_TRUE(r.get('n').has_value());
    EXPECT_EQ(r.get('n').value(), "");
}
