#pragma once

#include <regex.h>

namespace cfbox::util {

class scoped_regex {
    regex_t regex_{};
    bool valid_ = false;

public:
    scoped_regex() = default;
    ~scoped_regex() {
        if (valid_) regfree(&regex_);
    }
    scoped_regex(const scoped_regex&) = delete;
    scoped_regex& operator=(const scoped_regex&) = delete;

    auto compile(const char* pattern, int flags) -> int {
        if (valid_) { regfree(&regex_); valid_ = false; }
        int rc = regcomp(&regex_, pattern, flags);
        valid_ = (rc == 0);
        return rc;
    }

    auto exec(const char* str, std::size_t nmatch, regmatch_t* matches, int flags) const -> int {
        return regexec(&regex_, str, nmatch, matches, flags);
    }

    auto get() const -> const regex_t* { return &regex_; }
    auto valid() const -> bool { return valid_; }
};

} // namespace cfbox::util
