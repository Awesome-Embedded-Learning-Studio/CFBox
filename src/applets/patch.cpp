#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "patch",
    .version = CFBOX_VERSION_STRING,
    .one_line = "apply a diff file to an original",
    .usage   = "patch [FILE]",
    .options = "",
    .extra   = "",
};

struct Hunk {
    int old_start;
    int old_count;
    std::vector<std::string> lines; // '+', '-', ' ' prefixed
};

static auto parse_hunk_header(const std::string& line) -> Hunk {
    Hunk h{};
    h.old_start = 1;
    h.old_count = 0;
    // @@ -old_start[,old_count] +new_start[,new_count] @@
    auto p = line.c_str();
    while (*p && *p != '-') ++p;
    if (*p == '-') ++p;
    h.old_start = static_cast<int>(std::strtol(p, &const_cast<char*&>(p), 10));
    h.old_count = 1;
    if (*p == ',') { ++p; h.old_count = static_cast<int>(std::strtol(p, &const_cast<char*&>(p), 10)); }
    return h;
}

static auto is_diff_line(const std::string& s) -> bool {
    if (s.empty()) return false;
    return s[0] == ' ' || s[0] == '-' || s[0] == '+';
}
} // namespace

auto patch_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto patch_result = cfbox::io::read_all_stdin();
    if (!patch_result) {
        std::fprintf(stderr, "cfbox patch: %s\n", patch_result.error().msg.c_str());
        return 1;
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox patch: missing file operand\n");
        return 1;
    }

    std::string filepath{pos[0]};
    auto file_result = cfbox::io::read_lines(filepath);
    if (!file_result) {
        std::fprintf(stderr, "cfbox patch: %s\n", file_result.error().msg.c_str());
        return 1;
    }

    auto& lines = *file_result;
    auto patch_data = cfbox::io::split_lines(*patch_result);

    // Detect format: unified has @@, normal diff doesn't
    bool has_hunk_headers = false;
    for (const auto& l : patch_data) {
        if (l.starts_with("@@")) { has_hunk_headers = true; break; }
    }

    std::vector<Hunk> hunks;

    if (has_hunk_headers) {
        std::size_t i = 0;
        while (i < patch_data.size() && !patch_data[i].starts_with("@@")) ++i;

        while (i < patch_data.size()) {
            if (!patch_data[i].starts_with("@@")) { ++i; continue; }
            Hunk h = parse_hunk_header(patch_data[i]);
            ++i;
            int old_seen = 0;
            while (i < patch_data.size() && old_seen < h.old_count) {
                auto& pline = patch_data[i];
                if (pline.starts_with("@@")) break;
                if (!pline.empty()) {
                    h.lines.push_back(pline);
                    if (pline[0] == '-' || pline[0] == ' ') ++old_seen;
                }
                ++i;
            }
            // Also consume remaining '+' lines from this hunk
            while (i < patch_data.size() && !patch_data[i].starts_with("@@") &&
                   patch_data[i].starts_with("+")) {
                h.lines.push_back(patch_data[i]);
                ++i;
            }
            hunks.push_back(std::move(h));
        }
    } else {
        // Normal diff: treat entire input as one hunk at line 1
        Hunk h{};
        h.old_start = 1;
        for (const auto& pline : patch_data) {
            if (is_diff_line(pline)) {
                h.lines.push_back(pline);
            }
        }
        if (!h.lines.empty()) {
            h.old_count = 0;
            for (const auto& l : h.lines) {
                if (!l.empty() && (l[0] == '-' || l[0] == ' ')) ++h.old_count;
            }
            hunks.push_back(std::move(h));
        }
    }

    // Apply hunks in reverse order to preserve line numbers
    for (auto hi = hunks.rbegin(); hi != hunks.rend(); ++hi) {
        auto& h = *hi;
        int start = h.old_start - 1;
        if (start < 0) start = 0;

        int remove_count = 0;
        for (auto& l : h.lines) if (!l.empty() && l[0] != '+') ++remove_count;
        if (start + remove_count > static_cast<int>(lines.size())) {
            std::fprintf(stderr, "cfbox patch: hunk at line %d doesn't match\n", h.old_start);
            return 1;
        }

        std::vector<std::string> replacement;
        for (auto& l : h.lines) {
            if (l.empty()) continue;
            if (l[0] == ' ') replacement.emplace_back(l.substr(1));
            else if (l[0] == '+') replacement.emplace_back(l.substr(1));
        }

        lines.erase(lines.begin() + start, lines.begin() + start + remove_count);
        lines.insert(lines.begin() + start, replacement.begin(), replacement.end());
    }

    // Write result
    std::string output;
    for (const auto& l : lines) output += l + "\n";
    auto wresult = cfbox::io::write_all(filepath, output);
    if (!wresult) {
        std::fprintf(stderr, "cfbox patch: %s\n", wresult.error().msg.c_str());
        return 1;
    }
    return 0;
}
