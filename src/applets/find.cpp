// find — search for files in a directory hierarchy
// Predicates: -name PATTERN, -type [f|d|l|b|c|p|s], -maxdepth N, -exec CMD {} ;
// Boolean: -a (implicit), -o, ! / -not, ( ) grouping.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "find",
    .version = CFBOX_VERSION_STRING,
    .one_line = "search for files in a directory hierarchy",
    .usage   = "find [PATH] [PREDICATE]...",
    .options = "  Predicates:\n"
               "    -name PATTERN   match filename (glob * and ?)\n"
               "    -type [f|d|l|b|c|p|s]  match file type\n"
               "    -maxdepth N     descend at most N levels (global option)\n"
               "    -exec CMD {} ;  execute command on matches\n"
               "  Operators (descending precedence):\n"
               "    -o  logical OR   -a / implicit  logical AND\n"
               "    !  / -not        logical NOT     ( )  grouping",
    .extra   = "",
};

// fnmatch-style glob: supports * ? and literal chars.
auto glob_match(std::string_view pattern, std::string_view text) -> bool {
    std::size_t pi = 0, ti = 0;
    std::size_t star_pi = std::string_view::npos, star_ti = std::string_view::npos;

    while (ti < text.size()) {
        if (pi < pattern.size()) {
            if (pattern[pi] == '?') { ++pi; ++ti; continue; }
            if (pattern[pi] == '*') { star_pi = pi; star_ti = ti; ++pi; continue; }
            if (pattern[pi] == text[ti]) { ++pi; ++ti; continue; }
        }
        if (star_pi != std::string_view::npos) {
            pi = star_pi + 1;
            ti = star_ti + 1;
            star_ti = ti;
            continue;
        }
        return false;
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

auto file_type_char(const std::filesystem::directory_entry& entry) -> char {
    auto st = entry.symlink_status();
    switch (st.type()) {
        case std::filesystem::file_type::regular:    return 'f';
        case std::filesystem::file_type::directory:  return 'd';
        case std::filesystem::file_type::symlink:    return 'l';
        case std::filesystem::file_type::block:      return 'b';
        case std::filesystem::file_type::character:  return 'c';
        case std::filesystem::file_type::fifo:       return 'p';
        case std::filesystem::file_type::socket:     return 's';
        default: return '?';
    }
}

auto run_exec(const std::vector<std::string>& cmd_template,
              const std::string& filepath) -> void {
    std::vector<std::string> args;
    args.reserve(cmd_template.size());
    for (const auto& part : cmd_template) {
        args.push_back(part == "{}" ? filepath : part);
    }
    std::vector<char*> argv_arr;
    argv_arr.reserve(args.size() + 1);
    for (auto& a : args) argv_arr.push_back(a.data());
    argv_arr.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv_arr[0], argv_arr.data());
        _Exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

// Expression tree. And/Or hold N children; Not holds 1; Name/Type/Exec are
// leaves; True is a no-op (used for the empty expression and -maxdepth, which
// is a global option that does not participate in evaluation).
struct Node {
    enum Kind { And, Or, Not, Name, Type, Exec, True };
    Kind kind = True;
    std::string value;                     // Name pattern / Type char
    std::vector<std::string> exec_cmd;     // Exec command template
    std::vector<Node> children;            // And/Or/Not sub-expressions

    Node() = default;
    explicit Node(Kind k) : kind(k) {}
};

auto eval(const Node& n, const std::filesystem::directory_entry& entry) -> bool {
    switch (n.kind) {
        case Node::True:
            return true;
        case Node::Name:
            return glob_match(n.value, entry.path().filename().string());
        case Node::Type:
            return n.value.size() == 1 && file_type_char(entry) == n.value[0];
        case Node::Exec:
            run_exec(n.exec_cmd, entry.path().string());
            return true;  // actions always evaluate true
        case Node::Not:
            return !eval(n.children[0], entry);
        case Node::And:
            for (const auto& c : n.children)
                if (!eval(c, entry)) return false;
            return true;
        case Node::Or:
            for (const auto& c : n.children)
                if (eval(c, entry)) return true;
            return false;
    }
    return false;
}

auto has_exec_node(const Node& n) -> bool {
    if (n.kind == Node::Exec) return true;
    for (const auto& c : n.children)
        if (has_exec_node(c)) return true;
    return false;
}

// Recursive-descent parser over the predicate token stream. Sets `failed` on a
// syntax error (unknown predicate, missing operand, unbalanced paren).
struct Parser {
    const std::vector<std::string>& toks;
    int maxdepth = -1;   // global -maxdepth (out)
    std::size_t pos = 0;
    bool failed = false;

    [[nodiscard]] auto at_end() const -> bool { return pos >= toks.size(); }
    auto peek() const -> const std::string& { return toks[pos]; }

    auto parse_or() -> Node {
        Node left = parse_and();
        if (failed || at_end() || peek() != "-o") return left;
        Node node{Node::Or};
        node.children.push_back(std::move(left));
        while (!failed && !at_end() && peek() == "-o") {
            ++pos;
            node.children.push_back(parse_and());
        }
        return node;
    }

    auto parse_and() -> Node {
        Node left = parse_not();
        if (failed || at_end() || peek() == "-o" || peek() == ")") return left;
        Node node{Node::And};
        node.children.push_back(std::move(left));
        while (!failed && !at_end() && peek() != "-o" && peek() != ")") {
            if (peek() == "-a") ++pos;  // explicit; otherwise implicit AND
            node.children.push_back(parse_not());
        }
        return node;
    }

    auto parse_not() -> Node {
        if (!at_end() && (peek() == "!" || peek() == "-not")) {
            ++pos;
            Node node{Node::Not};
            node.children.push_back(parse_not());
            return node;
        }
        return parse_primary();
    }

    auto parse_primary() -> Node {
        if (at_end()) { failed = true; return Node{Node::True}; }
        const std::string& t = peek();

        if (t == "(") {
            ++pos;
            Node inner = parse_or();
            if (failed) return inner;
            if (at_end() || peek() != ")") { failed = true; return inner; }
            ++pos;
            return inner;
        }
        if (t == "-name") {
            ++pos;
            if (at_end()) { failed = true; return Node{Node::True}; }
            Node n{Node::Name};
            n.value = peek();
            ++pos;
            return n;
        }
        if (t == "-type") {
            ++pos;
            if (at_end()) { failed = true; return Node{Node::True}; }
            Node n{Node::Type};
            n.value = peek();
            ++pos;
            return n;
        }
        if (t == "-maxdepth") {
            ++pos;
            if (at_end()) { failed = true; return Node{Node::True}; }
            maxdepth = std::atoi(peek().c_str());
            ++pos;
            return Node{Node::True};  // global option: no-op in the tree
        }
        if (t == "-exec") {
            ++pos;
            Node n{Node::Exec};
            while (!at_end() && peek() != ";" && peek() != "\\;") {
                n.exec_cmd.push_back(peek());
                ++pos;
            }
            if (!at_end()) ++pos;  // consume the terminating ;
            return n;
        }

        failed = true;
        CFBOX_ERR("find", "unknown primary or operator '%s'", t.c_str());
        return Node{Node::True};
    }
};

auto do_find(const std::filesystem::path& root, const Node& expr, int maxdepth) -> int {
    const bool print = !has_exec_node(expr);

    std::error_code ec;
    auto it = std::filesystem::recursive_directory_iterator(
        root, std::filesystem::directory_options::follow_directory_symlink, ec);
    if (ec) {
        CFBOX_ERR("find", "'%s': %s", root.string().c_str(), ec.message().c_str());
        return 1;
    }

    auto emit = [&](const std::filesystem::directory_entry& entry, int depth) {
        if (maxdepth >= 0 && depth > maxdepth) return;
        if (eval(expr, entry) && print) {
            std::printf("%s\n", entry.path().string().c_str());
        }
        // When -exec is present, eval() already ran it as a side effect; the
        // default -print is suppressed.
    };

    // Evaluate the root entry itself (depth 0).
    {
        std::error_code ec2;
        std::filesystem::directory_entry root_entry(root, ec2);
        if (!ec2) emit(root_entry, 0);
    }

    int rc = 0;
    for (const auto& entry : it) {
        int depth = it.depth() + 1;  // root is depth 0
        if (maxdepth >= 0 && depth > maxdepth) {
            it.disable_recursion_pending();
            continue;
        }
        emit(entry, depth);
    }
    return rc;
}

} // namespace

auto find_main(int argc, char* argv[]) -> int {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--help")    { cfbox::help::print_help(HELP); return 0; }
        if (arg == "--version") { cfbox::help::print_version(HELP); return 0; }
    }

    // First non-option argument is the PATH, unless it begins an expression
    // (a predicate, '(', ')', or '!'). Default root is ".".
    int start = 1;
    std::filesystem::path root = ".";
    auto is_expr_start = [](const char* s) -> bool {
        return s[0] == '-' || s[0] == '(' || s[0] == ')' ||
               (s[0] == '!' && s[1] == '\0');
    };
    if (argc > 1 && !is_expr_start(argv[1])) {
        root = argv[1];
        start = 2;
    }

    std::vector<std::string> tokens;
    for (int i = start; i < argc; ++i) tokens.emplace_back(argv[i]);

    // Empty expression prints everything (root + descendants).
    if (tokens.empty()) {
        return do_find(root, Node{Node::True}, -1);
    }

    Parser parser{tokens};
    Node expr = parser.parse_or();
    if (parser.failed || !parser.at_end()) {
        return 1;  // GNU find: invalid expression -> exit 1
    }

    return do_find(root, expr, parser.maxdepth);
}
