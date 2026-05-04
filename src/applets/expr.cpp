#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/regex.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "expr",
    .version = CFBOX_VERSION_STRING,
    .one_line = "evaluate expressions",
    .usage   = "expr EXPRESSION...",
    .options = "",
    .extra   = "Supports: + - * / % < <= = != >= > : length substr index",
};
} // namespace

struct Value {
    enum Type { Int, Str } type;
    long ival = 0;
    std::string sval;

    static auto integer(long v) -> Value { return {Int, v, {}}; }
    static auto str(const std::string& s) -> Value {
        char* end = nullptr;
        long v = std::strtol(s.c_str(), &end, 10);
        if (*end == '\0' && !s.empty()) return {Int, v, {}};
        return {Str, 0, s};
    }
    auto to_int() const -> long {
        if (type == Int) return ival;
        char* end = nullptr;
        return std::strtol(sval.c_str(), &end, 10);
    }
    auto to_str() const -> std::string {
        if (type == Int) return std::to_string(ival);
        return sval;
    }
    auto is_zero_or_null() const -> bool {
        if (type == Int) return ival == 0;
        return sval.empty();
    }
};

static auto eval(std::vector<std::string>::iterator& it,
                 std::vector<std::string>::iterator end) -> Value;

static auto eval_primary(std::vector<std::string>::iterator& it,
                         std::vector<std::string>::iterator end) -> Value {
    if (it == end) return Value::str("");

    if (*it == "length" && std::next(it) != end) {
        ++it;
        auto v = eval_primary(it, end);
        return Value::integer(static_cast<long>(v.to_str().size()));
    }
    if (*it == "substr" && std::distance(it, end) >= 4) {
        ++it;
        auto s = eval_primary(it, end).to_str();
        auto pos = eval_primary(it, end).to_int();
        auto len = eval_primary(it, end).to_int();
        if (pos >= 1 && static_cast<std::size_t>(pos - 1) <= s.size()) {
            return Value::str(s.substr(static_cast<std::size_t>(pos - 1),
                                       static_cast<std::size_t>(len)));
        }
        return Value::str("");
    }
    if (*it == "index" && std::distance(it, end) >= 3) {
        ++it;
        auto s = eval_primary(it, end).to_str();
        auto chars = eval_primary(it, end).to_str();
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (chars.find(s[i]) != std::string::npos) {
                return Value::integer(static_cast<long>(i + 1));
            }
        }
        return Value::integer(0);
    }

    auto token = *it++;
    if (token == "(") {
        auto v = eval(it, end);
        if (it != end && *it == ")") ++it;
        return v;
    }
    return Value::str(token);
}

static auto eval_compare(std::vector<std::string>::iterator& it,
                         std::vector<std::string>::iterator end) -> Value {
    auto left = eval_primary(it, end);
    if (it != end) {
        auto op = *it;
        if (op == ":" && std::next(it) != end) {
            ++it;
            auto pattern = eval_primary(it, end).to_str();
            auto str = left.to_str();
            cfbox::util::scoped_regex regex;
            if (regex.compile(pattern.c_str(), REG_EXTENDED) != 0) {
                return Value::integer(0);
            }
            regmatch_t match;
            if (regex.exec(str.c_str(), 1, &match, 0) == 0) {
                if (match.rm_so >= 0 && match.rm_eo > match.rm_so) {
                    return Value::str(str.substr(
                        static_cast<std::size_t>(match.rm_so),
                        static_cast<std::size_t>(match.rm_eo - match.rm_so)));
                }
                return Value::integer(static_cast<long>(match.rm_eo - match.rm_so));
            }
            return Value::integer(0);
        }
        if ((op == "<" || op == "<=" || op == "=" || op == "==" ||
             op == "!=" || op == ">=" || op == ">") && std::next(it) != end) {
            ++it;
            auto right = eval_primary(it, end);
            bool result = false;
            if (left.type == Value::Int && right.type == Value::Int) {
                if (op == "<")  result = left.ival < right.ival;
                if (op == "<=") result = left.ival <= right.ival;
                if (op == "=" || op == "==") result = left.ival == right.ival;
                if (op == "!=") result = left.ival != right.ival;
                if (op == ">=") result = left.ival >= right.ival;
                if (op == ">")  result = left.ival > right.ival;
            } else {
                auto ls = left.to_str(), rs = right.to_str();
                if (op == "<")  result = ls < rs;
                if (op == "<=") result = ls <= rs;
                if (op == "=" || op == "==") result = ls == rs;
                if (op == "!=") result = ls != rs;
                if (op == ">=") result = ls >= rs;
                if (op == ">")  result = ls > rs;
            }
            return Value::integer(result ? 1 : 0);
        }
    }
    return left;
}

static auto eval_add(std::vector<std::string>::iterator& it,
                     std::vector<std::string>::iterator end) -> Value {
    auto left = eval_compare(it, end);
    while (it != end) {
        auto op = *it;
        if (op != "+" && op != "-") break;
        ++it;
        auto right = eval_compare(it, end);
        if (op == "+") left = Value::integer(left.to_int() + right.to_int());
        else           left = Value::integer(left.to_int() - right.to_int());
    }
    return left;
}

static auto eval(std::vector<std::string>::iterator& it,
                 std::vector<std::string>::iterator end) -> Value {
    auto left = eval_add(it, end);
    while (it != end) {
        auto op = *it;
        if (op == "*") {
            ++it; auto right = eval_add(it, end);
            left = Value::integer(left.to_int() * right.to_int());
        } else if (op == "/") {
            ++it; auto right = eval_add(it, end);
            if (right.to_int() == 0) {
                std::fprintf(stderr, "cfbox expr: division by zero\n");
                return Value::integer(0);
            }
            left = Value::integer(left.to_int() / right.to_int());
        } else if (op == "%") {
            ++it; auto right = eval_add(it, end);
            if (right.to_int() == 0) {
                std::fprintf(stderr, "cfbox expr: division by zero\n");
                return Value::integer(0);
            }
            left = Value::integer(left.to_int() % right.to_int());
        } else if (op == "|") {
            ++it;
            auto right = eval_add(it, end);
            if (!left.is_zero_or_null()) return left;
            left = right;
        } else if (op == "&") {
            ++it;
            auto right = eval_add(it, end);
            if (left.is_zero_or_null()) return left;
            left = right;
        } else {
            break;
        }
    }
    return left;
}

auto expr_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox expr: missing operand\n");
        return 2;
    }

    std::vector<std::string> args;
    for (auto p : pos) args.emplace_back(p);

    auto it = args.begin();
    auto result = eval(it, args.end());
    std::puts(result.to_str().c_str());
    return result.is_zero_or_null() ? 1 : 0;
}
