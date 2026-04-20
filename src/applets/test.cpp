#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "test",
    .version = CFBOX_VERSION_STRING,
    .one_line = "evaluate conditional expression",
    .usage   = "test EXPRESSION\n       [ EXPRESSION ]",
    .options = "",
    .extra   = "String:  -z STR, -n STR, STR1 = STR2, STR1 != STR2\n"
               "Integer: INT1 -eq/-ne/-lt/-le/-gt/-ge INT2\n"
               "File:    -e/-f/-d/-r/-w/-x/-s/-L FILE\n"
               "Logic:   ! EXPR, EXPR -a EXPR, EXPR -o EXPR, ( EXPR )",
};

using Args = std::vector<std::string_view>;

// Forward declarations
auto eval_expr(const Args& args) -> int;

auto to_int(std::string_view s) -> long {
    return std::strtol(std::string{s}.c_str(), nullptr, 10);
}

auto file_test(char op, std::string_view path) -> bool {
    struct stat st {};
    switch (op) {
    case 'e': return stat(std::string{path}.c_str(), &st) == 0;
    case 'f': return stat(std::string{path}.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    case 'd': return stat(std::string{path}.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    case 'r': return access(std::string{path}.c_str(), R_OK) == 0;
    case 'w': return access(std::string{path}.c_str(), W_OK) == 0;
    case 'x': return access(std::string{path}.c_str(), X_OK) == 0;
    case 's': return stat(std::string{path}.c_str(), &st) == 0 && st.st_size > 0;
    case 'L': return lstat(std::string{path}.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
    default: return false;
    }
}

// Evaluate with -o (lowest precedence)
auto eval_or(const Args& args) -> int;

// Evaluate with -a (higher precedence than -o)
auto eval_and(const Args& args) -> int;

auto eval_or(const Args& args) -> int {
    // Find first -o to split (lowest precedence)
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-o") {
            int left = eval_or(Args(args.begin(), args.begin() + static_cast<long>(i)));
            if (left == 0) return 0; // short-circuit: left is true
            return eval_and(Args(args.begin() + static_cast<long>(i) + 1, args.end()));
        }
    }
    return eval_and(args);
}

auto eval_and(const Args& args) -> int {
    // Find first -a to split (higher precedence than -o)
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-a") {
            int left = eval_and(Args(args.begin(), args.begin() + static_cast<long>(i)));
            if (left != 0) return 1; // short-circuit: left is false
            return eval_expr(Args(args.begin() + static_cast<long>(i) + 1, args.end()));
        }
    }
    return eval_expr(args);
}

auto eval_expr(const Args& args) -> int {
    if (args.empty()) return 1;

    // Parenthesized expression
    if (args[0] == "(" && args.size() >= 3 && args.back() == ")") {
        // Find matching close paren
        int depth = 0;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "(") ++depth;
            else if (args[i] == ")") {
                --depth;
                if (depth == 0 && i == args.size() - 1) {
                    return eval_or(Args(args.begin() + 1, args.end() - 1));
                }
            }
        }
    }

    // ! EXPR
    if (args[0] == "!" && args.size() > 1) {
        return eval_or(Args(args.begin() + 1, args.end())) == 0 ? 1 : 0;
    }

    // Unary tests (2 args)
    if (args.size() == 2) {
        if (args[0] == "-z") return args[1].empty() ? 0 : 1;
        if (args[0] == "-n") return args[1].empty() ? 1 : 0;
        if (args[0].size() == 2 && args[0][0] == '-') {
            char op = args[0][1];
            if (op == 'e' || op == 'f' || op == 'd' || op == 'r' ||
                op == 'w' || op == 'x' || op == 's' || op == 'L') {
                return file_test(op, args[1]) ? 0 : 1;
            }
        }
        std::fprintf(stderr, "cfbox test: unknown operator '%.*s'\n",
                     static_cast<int>(args[0].size()), args[0].data());
        return 2;
    }

    // Binary tests (3 args)
    if (args.size() == 3) {
        // String comparison
        if (args[1] == "=")  return args[0] == args[2] ? 0 : 1;
        if (args[1] == "!=") return args[0] != args[2] ? 0 : 1;

        // Integer comparison
        if (args[1] == "-eq") return to_int(args[0]) == to_int(args[2]) ? 0 : 1;
        if (args[1] == "-ne") return to_int(args[0]) != to_int(args[2]) ? 0 : 1;
        if (args[1] == "-lt") return to_int(args[0]) <  to_int(args[2]) ? 0 : 1;
        if (args[1] == "-le") return to_int(args[0]) <= to_int(args[2]) ? 0 : 1;
        if (args[1] == "-gt") return to_int(args[0]) >  to_int(args[2]) ? 0 : 1;
        if (args[1] == "-ge") return to_int(args[0]) >= to_int(args[2]) ? 0 : 1;

        std::fprintf(stderr, "cfbox test: unknown operator '%.*s'\n",
                     static_cast<int>(args[1].size()), args[1].data());
        return 2;
    }

    // Single arg: true if non-empty
    if (args.size() == 1) {
        return args[0].empty() ? 1 : 0;
    }

    // Fallback: try compound expression evaluation
    return eval_or(args);
}

} // namespace

auto test_main(int argc, char* argv[]) -> int {
    // Handle --help/--version before parsing
    // test uses positional args only, so we do manual check
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--help")    { cfbox::help::print_help(HELP); return 0; }
        if (arg == "--version") { cfbox::help::print_version(HELP); return 0; }
    }

    // Build expression from args (skip argv[0])
    Args expr_args;
    for (int i = 1; i < argc; ++i) {
        expr_args.emplace_back(argv[i]);
    }

    // If invoked as "[", last arg must be "]"
    std::string_view prog{argv[0]};
    if (!prog.empty() && prog.back() == '[') {
        // Also match when basename is "["
        auto slash = prog.rfind('/');
        auto base = (slash != std::string_view::npos) ? prog.substr(slash + 1) : prog;
        if (base == "[" || base == "[") {
            if (expr_args.empty() || expr_args.back() != "]") {
                std::fprintf(stderr, "cfbox [: missing ']'\n");
                return 2;
            }
            expr_args.pop_back();
        }
    }

    return eval_expr(expr_args);
}
