#include <cctype>
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "test",
    .version = CFBOX_VERSION_STRING,
    .one_line = "evaluate conditional expression",
    .usage   = "test EXPRESSION\n       [ EXPRESSION ]",
    .options = "",
    .extra   = "String:  -z STR, -n STR, STR1 = STR2, STR1 != STR2, STR1 < STR2, STR1 > STR2\n"
               "Integer: INT1 -eq/-ne/-lt/-le/-gt/-ge INT2\n"
               "File:    -e/-f/-d/-r/-w/-x/-s/-L/-h/-b/-c/-p/-S FILE\n"
               "File2:   FILE1 -nt/-ot/-ef FILE2\n"
               "Logic:   ! EXPR, EXPR -a EXPR, EXPR -o EXPR, ( EXPR )",
};

using Args = std::vector<std::string_view>;

// Parse a base-10 integer; the whole token must be consumed (no trailing junk).
// Returns nullopt for "abc", "5x", "" — POSIX test treats these as exit-2 errors.
// (std::strtol would silently coerce "abc" -> 0, masking invalid operands.)
auto parse_int(std::string_view s) -> std::optional<long> {
    if (s.empty()) return std::nullopt;
    long val = 0;
    auto res = std::from_chars(s.data(), s.data() + s.size(), val);
    if (res.ec != std::errc{} || res.ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return val;
}

auto file_test(char op, std::string_view path) -> bool {
    struct stat st {};
    switch (op) {
    case 'e': return stat(path.data(), &st) == 0;
    case 'f': return stat(path.data(), &st) == 0 && S_ISREG(st.st_mode);
    case 'd': return stat(path.data(), &st) == 0 && S_ISDIR(st.st_mode);
    case 'r': return access(path.data(), R_OK) == 0;
    case 'w': return access(path.data(), W_OK) == 0;
    case 'x': return access(path.data(), X_OK) == 0;
    case 's': return stat(path.data(), &st) == 0 && st.st_size > 0;
    case 'L': case 'h': return lstat(path.data(), &st) == 0 && S_ISLNK(st.st_mode);
    case 'b': return stat(path.data(), &st) == 0 && S_ISBLK(st.st_mode);
    case 'c': return stat(path.data(), &st) == 0 && S_ISCHR(st.st_mode);
    case 'p': return stat(path.data(), &st) == 0 && S_ISFIFO(st.st_mode);
    case 'S': return stat(path.data(), &st) == 0 && S_ISSOCK(st.st_mode);
    default: return false;
    }
}

// -nt: lhs exists and is newer (by mtime) than rhs; a missing rhs counts as
// infinitely old, so an existing lhs -nt a missing rhs is true.
auto newer_than(std::string_view lhs, std::string_view rhs) -> int {
    struct stat s1 {}, s2 {};
    if (stat(lhs.data(), &s1) != 0) return 1;          // lhs must exist
    if (stat(rhs.data(), &s2) != 0) return 0;          // rhs missing -> lhs newer
    return s1.st_mtime > s2.st_mtime ? 0 : 1;
}

auto older_than(std::string_view lhs, std::string_view rhs) -> int {
    struct stat s1 {}, s2 {};
    if (stat(lhs.data(), &s1) != 0) return 1;
    if (stat(rhs.data(), &s2) != 0) return 0;
    return s1.st_mtime < s2.st_mtime ? 0 : 1;
}

// -ef: same device + inode (hard links or the same file).
auto same_file(std::string_view lhs, std::string_view rhs) -> int {
    struct stat s1 {}, s2 {};
    if (stat(lhs.data(), &s1) != 0 || stat(rhs.data(), &s2) != 0) return 1;
    return (s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino) ? 0 : 1;
}

auto is_unary_op(std::string_view s) -> bool {
    if (s.size() != 2 || s[0] != '-') return false;
    switch (s[1]) {
    case 'z': case 'n': case 'e': case 'f': case 'd': case 'r':
    case 'w': case 'x': case 's': case 'L': case 'h':
    case 'b': case 'c': case 'p': case 'S':
        return true;
    default: return false;
    }
}

auto is_binary_op(std::string_view s) -> bool {
    return s == "=" || s == "!=" || s == "<" || s == ">"
        || s == "-eq" || s == "-ne" || s == "-lt" || s == "-le"
        || s == "-gt" || s == "-ge" || s == "-nt" || s == "-ot" || s == "-ef";
}

auto eval_unary(char op, std::string_view arg) -> int {
    if (op == 'z') return arg.empty() ? 0 : 1;
    if (op == 'n') return arg.empty() ? 1 : 0;
    return file_test(op, arg) ? 0 : 1;
}

auto eval_binary(std::string_view lhs, std::string_view op, std::string_view rhs) -> int {
    if (op == "=")  return lhs == rhs ? 0 : 1;
    if (op == "!=") return lhs != rhs ? 0 : 1;
    if (op == "<")  return lhs <  rhs ? 0 : 1;  // byte-wise (C locale)
    if (op == ">")  return lhs >  rhs ? 0 : 1;
    if (op == "-nt") return newer_than(lhs, rhs);
    if (op == "-ot") return older_than(lhs, rhs);
    if (op == "-ef") return same_file(lhs, rhs);

    // Integer comparison — both operands must be valid integers, else exit 2.
    auto l = parse_int(lhs);
    auto r = parse_int(rhs);
    if (!l || !r) {
        CFBOX_ERR("test", "integer expression expected");
        return 2;
    }
    if (op == "-eq") return *l == *r ? 0 : 1;
    if (op == "-ne") return *l != *r ? 0 : 1;
    if (op == "-lt") return *l <  *r ? 0 : 1;
    if (op == "-le") return *l <= *r ? 0 : 1;
    if (op == "-gt") return *l >  *r ? 0 : 1;
    if (op == "-ge") return *l >= *r ? 0 : 1;
    CFBOX_ERR("test", "unknown binary operator '%.*s'",
              static_cast<int>(op.size()), op.data());
    return 2;
}

// Recursive-descent evaluator over the token stream. Returns POSIX exit codes:
//   0 = true, 1 = false, 2 = syntax / operand error.
// Precedence: -o (lowest) < -a (implicit AND between primaries) < ! < primary.
struct Evaluator {
    const Args& a;
    std::size_t p = 0;

    [[nodiscard]] auto at_end() const -> bool { return p >= a.size(); }
    auto cur() const -> std::string_view { return a[p]; }

    auto parse_or() -> int {
        int r = parse_and();
        while (r != 2 && !at_end() && cur() == "-o") {
            ++p;
            int rhs = parse_and();
            if (rhs == 2) return 2;
            r = (r == 0 || rhs == 0) ? 0 : 1;  // OR
        }
        return r;
    }

    auto parse_and() -> int {
        int r = parse_not();
        // Continue while there is another primary (explicit -a or implicit AND).
        while (r != 2 && !at_end() && cur() != ")" && cur() != "-o") {
            if (cur() == "-a") ++p;  // explicit; otherwise adjacent primaries AND
            int rhs = parse_not();
            if (rhs == 2) return 2;
            r = (r == 0 && rhs == 0) ? 0 : 1;  // AND
        }
        return r;
    }

    auto parse_not() -> int {
        // "!" is negation only when an operand follows; a trailing "!" is a
        // non-empty string operand (POSIX: `test !` -> true).
        if (!at_end() && cur() == "!" && p + 1 < a.size()) {
            ++p;
            int r = parse_not();
            return r == 2 ? 2 : (r == 0 ? 1 : 0);  // NOT
        }
        return parse_primary();
    }

    auto parse_primary() -> int {
        if (at_end()) return 2;  // missing operand

        if (cur() == "(") {
            ++p;
            int r = parse_or();
            if (r == 2) return 2;
            if (at_end() || cur() != ")") return 2;  // unmatched "("
            ++p;
            return r;
        }

        // Unary: -OP ARG
        if (is_unary_op(cur())) {
            if (p + 1 >= a.size()) {
                CFBOX_ERR("test", "missing argument for '%.*s'",
                          static_cast<int>(cur().size()), cur().data());
                return 2;
            }
            char op = cur()[1];
            std::string_view arg = a[p + 1];
            p += 2;
            return eval_unary(op, arg);
        }

        // Binary: ARG OP ARG
        if (p + 2 < a.size() && is_binary_op(a[p + 1])) {
            std::string_view lhs = a[p];
            std::string_view op = a[p + 1];
            std::string_view rhs = a[p + 2];
            p += 3;
            return eval_binary(lhs, op, rhs);
        }

        // An unrecognized -X (alpha) is a syntax error, never a string operand.
        if (cur().size() >= 2 && cur()[0] == '-' &&
            std::isalpha(static_cast<unsigned char>(cur()[1]))) {
            CFBOX_ERR("test", "unknown operator '%.*s'",
                      static_cast<int>(cur().size()), cur().data());
            return 2;
        }

        // Single argument: true if non-empty.
        std::string_view s = cur();
        ++p;
        return s.empty() ? 1 : 0;
    }
};
} // namespace

auto test_main(int argc, char* argv[]) -> int {
    // test uses positional args only; intercept --help/--version manually.
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--help")    { cfbox::help::print_help(HELP); return 0; }
        if (arg == "--version") { cfbox::help::print_version(HELP); return 0; }
    }

    Args expr_args;
    for (int i = 1; i < argc; ++i) {
        expr_args.emplace_back(argv[i]);
    }

    // If invoked as "[", the last argument must be "]".
    std::string_view prog{argv[0]};
    auto slash = prog.rfind('/');
    auto base = (slash != std::string_view::npos) ? prog.substr(slash + 1) : prog;
    if (base == "[") {
        if (expr_args.empty() || expr_args.back() != "]") {
            CFBOX_ERR("[", "missing ']'");
            return 2;
        }
        expr_args.pop_back();
    }

    // POSIX: an empty expression ([ ] or bare test) is false, not an error.
    if (expr_args.empty()) return 1;

    Evaluator ev{expr_args, 0};
    int result = ev.parse_or();
    if (!ev.at_end()) {
        // Leftover tokens (e.g. a stray ")") mean the expression was malformed.
        return 2;
    }
    return result;
}
