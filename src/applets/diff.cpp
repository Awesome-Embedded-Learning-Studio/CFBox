#include <cstdio>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "diff",
    .version = CFBOX_VERSION_STRING,
    .one_line = "compare files line by line",
    .usage   = "diff [-u] FILE1 FILE2",
    .options = "  -u     output in unified format",
    .extra   = "",
};

static auto lcs_diff(const std::vector<std::string>& a, const std::vector<std::string>& b) -> void {
    auto m = a.size(), n = b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (std::size_t i = 1; i <= m; ++i)
        for (std::size_t j = 1; j <= n; ++j)
            dp[i][j] = (a[i-1] == b[j-1]) ? dp[i-1][j-1] + 1 : std::max(dp[i-1][j], dp[i][j-1]);

    std::vector<std::pair<char, std::string>> edits;
    std::size_t i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i-1] == b[j-1]) {
            edits.push_back({' ', a[i-1]});
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            edits.push_back({'+', b[j-1]});
            --j;
        } else {
            edits.push_back({'-', a[i-1]});
            --i;
        }
    }
    for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
        std::printf("%c%s\n", it->first, it->second.c_str());
    }
}

static auto unified_diff(const std::string& file1, const std::string& file2,
                         const std::vector<std::string>& a, const std::vector<std::string>& b) -> void {
    std::printf("--- %s\n+++ %s\n@@ -1,%zu +1,%zu @@\n", file1.c_str(), file2.c_str(), a.size(), b.size());
    auto m = a.size(), n = b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (std::size_t i = 1; i <= m; ++i)
        for (std::size_t j = 1; j <= n; ++j)
            dp[i][j] = (a[i-1] == b[j-1]) ? dp[i-1][j-1] + 1 : std::max(dp[i-1][j], dp[i][j-1]);

    std::vector<std::pair<char, std::string>> edits;
    std::size_t i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i-1] == b[j-1]) {
            edits.push_back({' ', a[i-1]});
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            edits.push_back({'+', b[j-1]});
            --j;
        } else {
            edits.push_back({'-', a[i-1]});
            --i;
        }
    }
    for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
        std::printf("%c%s\n", it->first, it->second.c_str());
    }
}
} // namespace

auto diff_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'u', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool unified = parsed.has('u');
    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox diff: missing operand\n");
        return 2;
    }

    auto a_result = cfbox::io::read_lines(std::string{pos[0]});
    auto b_result = cfbox::io::read_lines(std::string{pos[1]});
    if (!a_result) { std::fprintf(stderr, "cfbox diff: %s\n", a_result.error().msg.c_str()); return 2; }
    if (!b_result) { std::fprintf(stderr, "cfbox diff: %s\n", b_result.error().msg.c_str()); return 2; }

    if (*a_result == *b_result) return 0;

    if (unified) {
        unified_diff(std::string{pos[0]}, std::string{pos[1]}, *a_result, *b_result);
    } else {
        lcs_diff(*a_result, *b_result);
    }
    return 1;
}
