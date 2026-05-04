#include "sh.hpp"

#include <cstdlib>
#include <cstring>
#include <glob.h>
#include <memory>

namespace {

struct PipeCloser {
    void operator()(std::FILE* p) const noexcept {
        if (p) ::pclose(p);
    }
};
using unique_pipe = std::unique_ptr<std::FILE, PipeCloser>;

} // namespace

namespace cfbox::sh {

static auto expand_param(const std::string& name, const ShellState& state) -> std::string {
    // Handle ${VAR:-default}
    auto colon_pos = name.find(":-");
    if (colon_pos != std::string::npos) {
        auto var_name = name.substr(0, colon_pos);
        auto default_val = name.substr(colon_pos + 2);
        auto val = state.get_var(var_name);
        return val.empty() ? default_val : val;
    }
    // Handle ${VAR:+alt}
    colon_pos = name.find(":+");
    if (colon_pos != std::string::npos) {
        auto var_name = name.substr(0, colon_pos);
        auto alt_val = name.substr(colon_pos + 2);
        auto val = state.get_var(var_name);
        return val.empty() ? "" : alt_val;
    }
    return state.get_var(name);
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

} // namespace cfbox::sh
