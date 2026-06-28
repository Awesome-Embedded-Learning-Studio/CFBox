#include "sh.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fnmatch.h>
#include <glob.h>
#include <memory>

#include <cfbox/error.hpp>

namespace {

struct PipeCloser {
    void operator()(std::FILE* p) const noexcept {
        if (p) ::pclose(p);
    }
};
using unique_pipe = std::unique_ptr<std::FILE, PipeCloser>;

// Recursive-descent evaluator for $((expr)). Subset: + - * / %, comparisons,
// && || !, unary +/-, parentheses, integer literals, and variables (bare names
// or $VAR; empty/unset/non-numeric -> 0, matching shell semantics).
struct ArithEvaluator {
    std::string_view expr;
    std::size_t pos = 0;
    const cfbox::sh::ShellState& st;
    bool error = false;

    void skip_ws() {
        while (pos < expr.size() && std::isspace(static_cast<unsigned char>(expr[pos]))) ++pos;
    }
    [[nodiscard]] auto eof() const -> bool { return pos >= expr.size(); }

    auto match2(char a, char b) -> bool {
        skip_ws();
        if (pos + 1 < expr.size() && expr[pos] == a && expr[pos + 1] == b) { pos += 2; return true; }
        return false;
    }
    auto match(char c) -> bool {
        skip_ws();
        if (!eof() && expr[pos] == c) { ++pos; return true; }
        return false;
    }

    auto run() -> long { return parse_or(); }
    auto parse_or() -> long {
        auto l = parse_and();
        while (match2('|', '|')) { auto r = parse_and(); l = (l || r) ? 1 : 0; }
        return l;
    }
    auto parse_and() -> long {
        auto l = parse_eq();
        while (match2('&', '&')) { auto r = parse_eq(); l = (l && r) ? 1 : 0; }
        return l;
    }
    auto parse_eq() -> long {
        auto l = parse_cmp();
        for (;;) {
            if (match2('=', '=')) { l = (l == parse_cmp()) ? 1 : 0; }
            else if (match2('!', '=')) { l = (l != parse_cmp()) ? 1 : 0; }
            else break;
        }
        return l;
    }
    auto parse_cmp() -> long {
        auto l = parse_add();
        for (;;) {
            if (match2('<', '=')) { l = (l <= parse_add()) ? 1 : 0; }
            else if (match2('>', '=')) { l = (l >= parse_add()) ? 1 : 0; }
            else if (match('<')) { l = (l < parse_add()) ? 1 : 0; }
            else if (match('>')) { l = (l > parse_add()) ? 1 : 0; }
            else break;
        }
        return l;
    }
    auto parse_add() -> long {
        auto l = parse_mul();
        for (;;) {
            if (match('+')) l += parse_mul();
            else if (match('-')) l -= parse_mul();
            else break;
        }
        return l;
    }
    auto parse_mul() -> long {
        auto l = parse_unary();
        for (;;) {
            if (match('*')) l *= parse_unary();
            else if (match('/')) { auto r = parse_unary(); if (r == 0) { error = true; return 0; } l /= r; }
            else if (match('%')) { auto r = parse_unary(); if (r == 0) { error = true; return 0; } l %= r; }
            else break;
        }
        return l;
    }
    auto parse_unary() -> long {
        if (match('!')) return parse_unary() == 0 ? 1 : 0;
        if (match('-')) return -parse_unary();
        if (match('+')) return parse_unary();
        return parse_primary();
    }
    auto parse_primary() -> long {
        skip_ws();
        if (match('(')) {
            auto v = parse_or();
            match(')');
            return v;
        }
        if (!eof() && std::isdigit(static_cast<unsigned char>(expr[pos]))) {
            long v = 0;
            while (!eof() && std::isdigit(static_cast<unsigned char>(expr[pos]))) {
                v = v * 10 + (expr[pos] - '0');
                ++pos;
            }
            return v;
        }
        std::string name;
        if (!eof() && expr[pos] == '$') {
            ++pos;
            if (!eof() && expr[pos] == '{') {
                ++pos;
                while (!eof() && expr[pos] != '}') name += expr[pos++];
                if (!eof()) ++pos;
            } else {
                while (!eof() && (std::isalnum(static_cast<unsigned char>(expr[pos])) || expr[pos] == '_')) name += expr[pos++];
            }
        } else if (!eof() && (std::isalpha(static_cast<unsigned char>(expr[pos])) || expr[pos] == '_')) {
            while (!eof() && (std::isalnum(static_cast<unsigned char>(expr[pos])) || expr[pos] == '_')) name += expr[pos++];
        }
        if (name.empty()) { error = true; return 0; }
        auto val = st.get_var(name);
        if (val.empty()) return 0;
        char* end = nullptr;
        long n = std::strtol(val.c_str(), &end, 10);
        return (*end == '\0') ? n : 0;
    }
};

auto eval_arith(const std::string& expr, const cfbox::sh::ShellState& state) -> std::string {
    ArithEvaluator ae{expr, 0, state};
    auto v = ae.run();
    if (ae.error) {
        CFBOX_ERR("sh", "arithmetic expression error");
        return "0";
    }
    return std::to_string(v);
}

// Length of the shortest/longest prefix of val that matches glob pat (0 = none).
auto glob_prefix_len(const std::string& pat, const std::string& val, bool longest) -> std::size_t {
    if (longest) {
        for (std::size_t k = val.size(); k > 0; --k)
            if (::fnmatch(pat.c_str(), val.substr(0, k).c_str(), 0) == 0) return k;
    } else {
        for (std::size_t k = 1; k <= val.size(); ++k)
            if (::fnmatch(pat.c_str(), val.substr(0, k).c_str(), 0) == 0) return k;
    }
    return 0;
}

// Length of the shortest/longest suffix of val that matches glob pat (0 = none).
auto glob_suffix_len(const std::string& pat, const std::string& val, bool longest) -> std::size_t {
    if (longest) {
        for (std::size_t k = val.size(); k > 0; --k)
            if (::fnmatch(pat.c_str(), val.substr(val.size() - k).c_str(), 0) == 0) return k;
    } else {
        for (std::size_t k = 1; k <= val.size(); ++k)
            if (::fnmatch(pat.c_str(), val.substr(val.size() - k).c_str(), 0) == 0) return k;
    }
    return 0;
}

} // namespace

namespace cfbox::sh {

static auto expand_param(const std::string& name, const ShellState& state) -> std::string {
    // ${#NAME} -> length of NAME's value.
    if (name.size() >= 2 && name[0] == '#') {
        return std::to_string(state.get_var(name.substr(1)).size());
    }

    // Locate the operator following the variable name ([A-Za-z0-9_]+).
    std::size_t i = 0;
    while (i < name.size() &&
           (std::isalnum(static_cast<unsigned char>(name[i])) || name[i] == '_')) {
        ++i;
    }
    if (i == 0 || i >= name.size()) {
        return state.get_var(name);  // plain ${VAR}, no operator
    }

    const bool colon = (name[i] == ':');
    const std::size_t op_idx = colon ? i + 1 : i;
    if (op_idx >= name.size()) return state.get_var(name);

    const char opc = name[op_idx];
    const bool dbl = (op_idx + 1 < name.size() && name[op_idx + 1] == opc &&
                      (opc == '#' || opc == '%'));
    const std::string var_name = name.substr(0, i);
    const std::string arg = name.substr(op_idx + (dbl ? 2 : 1));
    const std::string val = state.get_var(var_name);

    switch (opc) {
    case '#': {  // strip matching prefix (# shortest, ## longest)
        return val.substr(glob_prefix_len(arg, val, dbl));
    }
    case '%': {  // strip matching suffix (% shortest, %% longest)
        return val.substr(0, val.size() - glob_suffix_len(arg, val, dbl));
    }
    case '-':  // default: empty/unset -> arg
        return val.empty() ? arg : val;
    case '+':  // alternative: non-empty -> arg
        return val.empty() ? std::string{} : arg;
    default:
        return val;
    }
}

// Process $ expansions in a word fragment, returns the expanded string
template<typename Iter>
static auto process_dollar(Iter& it, Iter end, const ShellState& state) -> std::string {
    if (it == end) return "$";
    ++it; // skip $

    if (it == end) return "$";

    char c = *it;

    if (c == '{') {
        ++it; // skip {
        std::string name;
        while (it != end && *it != '}') {
            name += *it++;
        }
        if (it != end) ++it; // skip }
        return expand_param(name, state);
    }

    if (c == '(') {
        // $((expr)) arithmetic, distinguished from $(...) command substitution.
        auto next_it = it;
        ++next_it;
        if (next_it != end && *next_it == '(') {
            ++it; ++it;  // skip both (
            int depth = 0;
            std::string expr;
            while (it != end) {
                char ch = *it;
                if (ch == '(') { ++depth; expr += ch; }
                else if (ch == ')') {
                    if (depth == 0) {
                        ++it;  // first ) of ))
                        if (it != end && *it == ')') ++it;  // second )
                        break;
                    }
                    --depth;
                    expr += ch;
                } else {
                    expr += ch;
                }
                ++it;
            }
            return eval_arith(expr, state);
        }
        // $(...) — basic command substitution support
        ++it; // skip (
        int depth = 1;
        std::string cmd;
        while (it != end && depth > 0) {
            if (*it == '(') ++depth;
            else if (*it == ')') { --depth; if (depth == 0) { ++it; break; } }
            if (depth > 0) cmd += *it;
            ++it;
        }
        // Execute via popen
        std::string result;
        unique_pipe pipe(::popen(cmd.c_str(), "r"));
        if (pipe) {
            char buf[256];
            while (std::fgets(buf, sizeof(buf), pipe.get())) {
                result += buf;
            }
            // Strip trailing newline
            while (!result.empty() && result.back() == '\n') result.pop_back();
        }
        return result;
    }

    // Special parameters
    if (c == '?' || c == '$' || c == '!' || c == '#' || c == '@' || c == '*') {
        ++it;
        std::string name(1, c);
        return state.get_var(name);
    }

    // Positional $0-$9
    if (c >= '0' && c <= '9') {
        ++it;
        return state.get_var(std::string(1, c));
    }

    // Variable name
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::string name;
        while (it != end && (std::isalnum(static_cast<unsigned char>(*it)) || *it == '_')) {
            name += *it++;
        }
        return state.get_var(name);
    }

    return "$";
}

// Expand a single word (may produce multiple words via field splitting + glob)
auto expand_word(const std::string& word, const ShellState& state) -> std::vector<std::string> {
    std::string expanded;
    bool in_double_quotes = false;

    auto it = word.cbegin();
    auto end = word.cend();

    while (it != end) {
        char c = *it;

        if (c == '"') {
            in_double_quotes = !in_double_quotes;
            ++it;
            continue;
        }

        if (c == '\\' && !in_double_quotes) {
            ++it;
            if (it != end) { expanded += *it++; }
            continue;
        }

        if (c == '$') {
            expanded += process_dollar(it, end, state);
            continue;
        }

        if (c == '~' && !in_double_quotes && expanded.empty()) {
            // Tilde expansion
            ++it;
            std::string user;
            while (it != end && *it != '/' && *it != ':' && *it != ' ' && *it != '"') {
                user += *it++;
            }
            if (user.empty()) {
                const char* home = std::getenv("HOME");
                expanded += home ? home : "/root";
            } else {
                // ~user — lookup via getpwnam (simplified: not implemented)
                expanded += "~";
                expanded += user;
            }
            continue;
        }

        expanded += c;
        ++it;
    }

    // If the word was quoted, don't do field splitting or globbing
    if (in_double_quotes || word.starts_with("'") || word.find('"') != std::string::npos) {
        // Keep as single word (already expanded)
        return {expanded};
    }

    // Field splitting on IFS (simplified: split on whitespace if unquoted spaces from expansion)
    // For MVP: if no spaces in expanded, treat as single word
    bool has_spaces = expanded.find(' ') != std::string::npos ||
                      expanded.find('\t') != std::string::npos;

    if (!has_spaces) {
        // Try glob expansion
        glob_t globbuf {};
        int rc = ::glob(expanded.c_str(), GLOB_NOSORT | GLOB_NOESCAPE, nullptr, &globbuf);
        if (rc == 0 && globbuf.gl_pathc > 0) {
            std::vector<std::string> result;
            for (std::size_t i = 0; i < globbuf.gl_pathc; ++i) {
                result.emplace_back(globbuf.gl_pathv[i]);
            }
            ::globfree(&globbuf);
            return result;
        }
        ::globfree(&globbuf);
        return {expanded};
    }

    // Split on whitespace
    std::vector<std::string> result;
    std::string current;
    for (char ch : expanded) {
        if (ch == ' ' || ch == '\t') {
            if (!current.empty()) {
                result.push_back(std::move(current));
                current.clear();
            }
        } else {
            current += ch;
        }
    }
    if (!current.empty()) result.push_back(std::move(current));

    // Glob each resulting word
    std::vector<std::string> globbed;
    for (auto& w : result) {
        glob_t globbuf {};
        int rc = ::glob(w.c_str(), GLOB_NOSORT | GLOB_NOESCAPE, nullptr, &globbuf);
        if (rc == 0 && globbuf.gl_pathc > 0) {
            for (std::size_t i = 0; i < globbuf.gl_pathc; ++i) {
                globbed.emplace_back(globbuf.gl_pathv[i]);
            }
        } else {
            globbed.push_back(std::move(w));
        }
        ::globfree(&globbuf);
    }
    return globbed;
}

auto expand_words(const std::vector<std::string>& words, const ShellState& state) -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto& w : words) {
        auto expanded = expand_word(w, state);
        for (auto& e : expanded) {
            result.push_back(std::move(e));
        }
    }
    return result;
}

// Expand param/arith/command substitution and quotes, but skip field splitting
// and filename globbing. Used for `case` words and patterns, where the glob
// metacharacters (* ? [ ]) are pattern syntax rather than filename globs.
auto expand_noglob(const std::string& word, const ShellState& state) -> std::string {
    std::string expanded;
    bool in_double_quotes = false;
    auto it = word.cbegin();
    auto end = word.cend();
    while (it != end) {
        char c = *it;
        if (c == '"') { in_double_quotes = !in_double_quotes; ++it; continue; }
        if (c == '\\' && !in_double_quotes) { ++it; if (it != end) { expanded += *it++; } continue; }
        if (c == '$') { expanded += process_dollar(it, end, state); continue; }
        if (c == '~' && !in_double_quotes && expanded.empty()) {
            ++it;
            const char* home = std::getenv("HOME");
            expanded += home ? home : "/root";
            continue;
        }
        expanded += c;
        ++it;
    }
    return expanded;
}

} // namespace cfbox::sh
