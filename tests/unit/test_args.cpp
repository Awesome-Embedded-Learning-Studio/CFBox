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

// ── long option: bool flag ───────────────────────────────────

TEST(ArgsTest, LongBoolFlag) {
    char a0[] = "prog", a1[] = "--recursive";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'r', false, "recursive"}});
    EXPECT_TRUE(r.has('r'));
    EXPECT_TRUE(r.has_long("recursive"));
}

TEST(ArgsTest, LongBoolFlagHasAny) {
    char a0[] = "prog", a1[] = "--recursive";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'r', false, "recursive"}});
    EXPECT_TRUE(r.has_any('r', "recursive"));
}

TEST(ArgsTest, ShortFlagHasAny) {
    char a0[] = "prog", a1[] = "-r";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'r', false, "recursive"}});
    EXPECT_TRUE(r.has_any('r', "recursive"));
}

// ── long option: value via = ──────────────────────────────────

TEST(ArgsTest, LongValueEquals) {
    char a0[] = "prog", a1[] = "--name=foo";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'n', true, "name"}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_TRUE(r.get('n').has_value());
    EXPECT_EQ(r.get('n').value(), "foo");
    EXPECT_TRUE(r.has_long("name"));
    ASSERT_TRUE(r.get_long("name").has_value());
    EXPECT_EQ(r.get_long("name").value(), "foo");
}

// ── long option: value via separate arg ───────────────────────

TEST(ArgsTest, LongValueSeparate) {
    char a0[] = "prog", a1[] = "--name", a2[] = "bar";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'n', true, "name"}});
    EXPECT_TRUE(r.has('n'));
    ASSERT_TRUE(r.get('n').has_value());
    EXPECT_EQ(r.get('n').value(), "bar");
}

// ── unregistered long option (--help, --version) ─────────────

TEST(ArgsTest, UnregisteredLongOption) {
    char a0[] = "prog", a1[] = "--help";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {});
    EXPECT_TRUE(r.has_long("help"));
    EXPECT_FALSE(r.get_long("help").has_value());
}

TEST(ArgsTest, UnregisteredLongOptionWithValue) {
    char a0[] = "prog", a1[] = "--version";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {});
    EXPECT_TRUE(r.has_long("version"));
}

// ── long + short mixed ───────────────────────────────────────

TEST(ArgsTest, LongAndShortMixed) {
    char a0[] = "prog", a1[] = "-r", a2[] = "--force", a3[] = "file";
    char* argv[] = {a0, a1, a2, a3};
    auto r = parse(4, argv, {OptSpec{'r', false, "recursive"}, OptSpec{'f', false, "force"}});
    EXPECT_TRUE(r.has('r'));
    EXPECT_TRUE(r.has_long("force"));
    EXPECT_TRUE(r.has('f'));
    ASSERT_EQ(r.positional().size(), 1u);
    EXPECT_EQ(r.positional()[0], "file");
}

// ── get_any works for both forms ─────────────────────────────

TEST(ArgsTest, GetAnyLongForm) {
    char a0[] = "prog", a1[] = "--mode=fast";
    char* argv[] = {a0, a1};
    auto r = parse(2, argv, {OptSpec{'m', true, "mode"}});
    ASSERT_TRUE(r.get_any('m', "mode").has_value());
    EXPECT_EQ(r.get_any('m', "mode").value(), "fast");
}

TEST(ArgsTest, GetAnyShortForm) {
    char a0[] = "prog", a1[] = "-m", a2[] = "fast";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'m', true, "mode"}});
    ASSERT_TRUE(r.get_any('m', "mode").has_value());
    EXPECT_EQ(r.get_any('m', "mode").value(), "fast");
}

// ── double dash still stops with long option specs ────────────

TEST(ArgsTest, DoubleDashStopsWithLongOptions) {
    char a0[] = "prog", a1[] = "--", a2[] = "--recursive";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'r', false, "recursive"}});
    EXPECT_FALSE(r.has('r'));
    EXPECT_FALSE(r.has_long("recursive"));
    ASSERT_EQ(r.positional().size(), 1u);
    EXPECT_EQ(r.positional()[0], "--recursive");
}

// ── OptSpec backward compatibility: no long_name still works ─

TEST(ArgsTest, OptSpecNoLongNameBackCompat) {
    char a0[] = "prog", a1[] = "-n", a2[] = "hello";
    char* argv[] = {a0, a1, a2};
    auto r = parse(3, argv, {OptSpec{'n', false}});
    EXPECT_TRUE(r.has('n'));
    EXPECT_EQ(r.positional()[0], "hello");
}
