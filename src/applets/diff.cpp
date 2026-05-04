#include <algorithm>
#include <cstdio>
#include <cstring>
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

struct Edit {
    char op; // ' ', '+', '-'
    std::size_t line; // line content index (a for ' '/'-', b for '+')
};

// Myers O(ND) diff — compute shortest edit script
static auto myers_diff(const std::vector<std::string>& a, const std::vector<std::string>& b)
    -> std::vector<Edit> {
    auto N = static_cast<int>(a.size());
    auto M = static_cast<int>(b.size());
    if (N == 0 && M == 0) return {};

    // Simple cases: one side empty
    if (N == 0) {
        std::vector<Edit> e;
        for (int j = 0; j < M; ++j) e.push_back({'+', static_cast<std::size_t>(j)});
        return e;
    }
    if (M == 0) {
        std::vector<Edit> e;
        for (int i = 0; i < N; ++i) e.push_back({'-', static_cast<std::size_t>(i)});
        return e;
    }

    // Forward pass with V-trace storage
    int max_d = N + M;
    int off = max_d; // offset to make k index non-negative
    // Store complete V array at each d
    std::vector<std::vector<int>> vv;

    {
        std::vector<int> v(static_cast<std::size_t>(2 * max_d + 1), 0);
        v[static_cast<std::size_t>(1 + off)] = 0;

        for (int d = 0; d <= max_d; ++d) {
            std::vector<int> prev = v;
            for (int k = -d; k <= d; k += 2) {
                int x;
                if (k == -d || (k != d && prev[static_cast<std::size_t>(k - 1 + off)] < prev[static_cast<std::size_t>(k + 1 + off)])) {
                    x = prev[static_cast<std::size_t>(k + 1 + off)];
                } else {
                    x = prev[static_cast<std::size_t>(k - 1 + off)] + 1;
                }
                int y = x - k;
                while (x < N && y < M && a[static_cast<std::size_t>(x)] == b[static_cast<std::size_t>(y)]) {
                    ++x; ++y;
                }
                v[static_cast<std::size_t>(k + off)] = x;
                if (x >= N && y >= M) {
                    vv.push_back(v);
                    goto forward_done;
                }
            }
            vv.push_back(v);
        }
    }
    forward_done:

    // Backtrack through vv to recover edit script
    std::vector<Edit> edits;
    int x = N, y = M;

    for (int d = static_cast<int>(vv.size()) - 1; d > 0; --d) {
        int k = x - y;
        auto& prev = vv[static_cast<std::size_t>(d - 1)];

        // Determine if we came from k+1 (insert) or k-1 (delete)
        bool from_below = (k == -d) ||
            (k != d && prev[static_cast<std::size_t>(k - 1 + off)] < prev[static_cast<std::size_t>(k + 1 + off)]);

        int mid_x, mid_y; // position after the non-diagonal step
        if (from_below) {
            mid_x = prev[static_cast<std::size_t>(k + 1 + off)];
            mid_y = mid_x - (k + 1);
        } else {
            mid_x = prev[static_cast<std::size_t>(k - 1 + off)] + 1;
            mid_y = mid_x - (k - 1);
        }

        // Record diagonal steps (equal lines) from (x,y) back to (mid_x, mid_y)
        while (x > mid_x && y > mid_y) {
            --x; --y;
            edits.push_back({' ', static_cast<std::size_t>(x)});
        }

        // Record the non-diagonal step
        if (from_below) {
            // insert b[y-1] — but after the step, we're at (mid_x, mid_y) = (prev[k+1], prev[k+1]-(k+1))
            // The step moved from (mid_x, mid_y+1) down to (mid_x, mid_y)
            edits.push_back({'+', static_cast<std::size_t>(mid_y)}); // b[mid_y] was inserted
            --y; // adjust to position before insert
        } else {
            // delete a[x-1]
            edits.push_back({'-', static_cast<std::size_t>(mid_x - 1)}); // a[mid_x-1] was deleted
            --x; // adjust to position before delete
        }

        // Now (x,y) should match prev[k'] where k' is the diagonal we came from
    }

    // d=0: only diagonal steps from (x,y) to (0,0)
    while (x > 0 && y > 0) {
        --x; --y;
        edits.push_back({' ', static_cast<std::size_t>(x)});
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
}

static auto print_edits(const std::vector<Edit>& edits,
                        const std::vector<std::string>& a,
                        const std::vector<std::string>& b) -> void {
    for (auto& e : edits) {
        if (e.op == ' ' || e.op == '-') {
            std::printf("%c%s\n", e.op, a[e.line].c_str());
        } else {
            std::printf("+%s\n", b[e.line].c_str());
        }
    }
}

struct Hunk {
    int a_start, a_count;
    int b_start, b_count;
    std::vector<Edit> edits;
};

static auto build_hunks(const std::vector<Edit>& edits,
                        int context = 3) -> std::vector<Hunk> {
    if (edits.empty()) return {};

    // Find change positions
    std::vector<int> change_idx;
    for (int i = 0; i < static_cast<int>(edits.size()); ++i) {
        if (edits[static_cast<std::size_t>(i)].op != ' ')
            change_idx.push_back(i);
    }
    if (change_idx.empty()) return {};

    // Group changes into hunks with context
    std::vector<Hunk> hunks;
    int hunk_start = std::max(0, change_idx[0] - context);

    for (int ci = 1; ci < static_cast<int>(change_idx.size()); ++ci) {
        int gap_start = change_idx[static_cast<std::size_t>(ci - 1)] + 1;
        int gap_end = change_idx[static_cast<std::size_t>(ci)] - 1;
        // If gap between changes exceeds 2*context, split into new hunk
        if (gap_end - gap_start + 1 > 2 * context) {
            int hunk_end = std::min(static_cast<int>(edits.size()) - 1,
                                    change_idx[static_cast<std::size_t>(ci - 1)] + context);
            Hunk h;
            h.edits.assign(edits.begin() + hunk_start, edits.begin() + hunk_end + 1);
            // Count a/b lines for this hunk
            h.a_start = 1; h.a_count = 0;
            h.b_start = 1; h.b_count = 0;
            bool a_init = false, b_init = false;
            for (auto& e : h.edits) {
                if (e.op == ' ' || e.op == '-') {
                    if (!a_init) { h.a_start = static_cast<int>(e.line) + 1; a_init = true; }
                    ++h.a_count;
                }
                if (e.op == ' ' || e.op == '+') {
                    if (!b_init) { h.b_start = static_cast<int>(e.line) + 1; b_init = true; }
                    ++h.b_count;
                }
            }
            hunks.push_back(std::move(h));
            hunk_start = std::max(0, change_idx[static_cast<std::size_t>(ci)] - context);
        }
    }
    // Last hunk
    int hunk_end = std::min(static_cast<int>(edits.size()) - 1,
                            change_idx.back() + context);
    Hunk h;
    h.edits.assign(edits.begin() + hunk_start, edits.begin() + hunk_end + 1);
    h.a_start = 1; h.a_count = 0;
    h.b_start = 1; h.b_count = 0;
    bool a_init = false, b_init = false;
    for (auto& e : h.edits) {
        if (e.op == ' ' || e.op == '-') {
            if (!a_init) { h.a_start = static_cast<int>(e.line) + 1; a_init = true; }
            ++h.a_count;
        }
        if (e.op == ' ' || e.op == '+') {
            if (!b_init) { h.b_start = static_cast<int>(e.line) + 1; b_init = true; }
            ++h.b_count;
        }
    }
    hunks.push_back(std::move(h));
    return hunks;
}

static auto unified_diff(const std::string& file1, const std::string& file2,
                         const std::vector<std::string>& a, const std::vector<std::string>& b) -> void {
    std::printf("--- %s\n+++ %s\n", file1.c_str(), file2.c_str());
    auto edits = myers_diff(a, b);
    auto hunks = build_hunks(edits);
    for (auto& h : hunks) {
        std::printf("@@ -%d,%d +%d,%d @@\n",
                    h.a_start, h.a_count, h.b_start, h.b_count);
        for (auto& e : h.edits) {
            if (e.op == ' ' || e.op == '-')
                std::printf("%c%s\n", e.op, a[e.line].c_str());
            else
                std::printf("+%s\n", b[e.line].c_str());
        }
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
        auto edits = myers_diff(*a_result, *b_result);
        print_edits(edits, *a_result, *b_result);
    }
    return 1;
}
