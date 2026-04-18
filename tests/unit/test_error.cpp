#include <cfbox/error.hpp>
#include <gtest/gtest.h>

using namespace cfbox::base;

// ── helpers ──────────────────────────────────────────────────

static auto ok_value() -> Result<int> {
    return 42;
}

static auto err_value() -> Result<int> {
    return std::unexpected(Error{1, "bad"});
}

static auto try_propagate() -> Result<int> {
    CFBOX_TRY(res, err_value());
    return std::move(res).value() + 1; // should not reach
}

static auto try_ok_chain() -> Result<int> {
    CFBOX_TRY(res, ok_value());
    return std::move(res).value() + 8; // 42 + 8 = 50
}

// ── Error struct ─────────────────────────────────────────────

TEST(ErrorTest, ErrorFields) {
    Error e{2, "not found"};
    EXPECT_EQ(e.code, 2);
    EXPECT_EQ(e.msg, "not found");
}

TEST(ErrorTest, ErrorMove) {
    Error a{3, "moved"};
    Error b = std::move(a);
    EXPECT_EQ(b.code, 3);
    EXPECT_EQ(b.msg, "moved");
}

// ── ErrorView struct ─────────────────────────────────────────

TEST(ErrorTest, ErrorViewFields) {
    constexpr int code = 4;
    ErrorView ev{code, "view msg"};
    EXPECT_EQ(ev.code, 4);
    EXPECT_EQ(ev.msg, "view msg");
}

// ── Result<T> ok path ────────────────────────────────────────

TEST(ErrorTest, ResultOk) {
    auto r = ok_value();
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 42);
    EXPECT_EQ(*r, 42);
}

// ── Result<T> error path ─────────────────────────────────────

TEST(ErrorTest, ResultErr) {
    auto r = err_value();
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, 1);
    EXPECT_EQ(r.error().msg, "bad");
}

// ── CFBOX_TRY propagation ────────────────────────────────────

TEST(ErrorTest, TryPropagatesError) {
    auto r = try_propagate();
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, 1);
    EXPECT_EQ(r.error().msg, "bad");
}

TEST(ErrorTest, TryPassesValueThrough) {
    auto r = try_ok_chain();
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 50);
}
