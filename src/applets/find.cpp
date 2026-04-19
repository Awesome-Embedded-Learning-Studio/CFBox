// find — search for files in a directory hierarchy
// Supported predicates: -name PATTERN, -type [f|d|l], -maxdepth N, -exec CMD {} ;
// Known differences from GNU find: no -iname, -path, -perm, -mtime, -newer,
// -delete, -print0, or complex boolean expressions. -name uses fnmatch-style glob.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

// Simple fnmatch-style glob matching: supports * ? and literal chars
auto glob_match(std::string_view pattern, std::string_view text) -> bool {
    std::size_t pi = 0, ti = 0;
    std::size_t star_pi = std::string_view::npos, star_ti = std::string_view::npos;

    while (ti < text.size()) {
        if (pi < pattern.size()) {
            if (pattern[pi] == '?') {
                ++pi; ++ti;
                continue;
            }
            if (pattern[pi] == '*') {
                star_pi = pi;
                star_ti = ti;
                ++pi;
                continue;
            }
            if (pattern[pi] == text[ti]) {
                ++pi; ++ti;
                continue;
            }
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

struct Predicate {
    enum Type { Name, TypeMatch, MaxDepth, Exec };
    Type type = Name;
    std::string value;        // for -name pattern, -type char
    int numeric = 0;          // for -maxdepth
    std::vector<std::string> exec_cmd; // for -exec
};

auto parse_predicates(int argc, char* argv[], int start) -> std::vector<Predicate> {
    std::vector<Predicate> preds;
    int i = start;
    while (i < argc) {
        std::string_view arg{argv[i]};
        if (arg == "-name" && i + 1 < argc) {
            preds.push_back({});
            preds.back().type = Predicate::Name;
            preds.back().value = argv[i + 1];
            i += 2;
        } else if (arg == "-type" && i + 1 < argc) {
            preds.push_back({});
            preds.back().type = Predicate::TypeMatch;
            preds.back().value = argv[i + 1];
            i += 2;
        } else if (arg == "-maxdepth" && i + 1 < argc) {
            preds.push_back({});
            preds.back().type = Predicate::MaxDepth;
            preds.back().numeric = std::atoi(argv[i + 1]);
            i += 2;
        } else if (arg == "-exec") {
            ++i;
            Predicate p;
            p.type = Predicate::Exec;
            while (i < argc) {
                std::string_view token{argv[i]};
                if (token == ";" || token == "\\;") break;
                p.exec_cmd.push_back(std::string{token});
                ++i;
            }
            // skip the terminating ;
            if (i < argc) ++i;
            preds.push_back(std::move(p));
        } else {
            ++i;
        }
    }
    return preds;
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

auto matches_predicates(const std::filesystem::directory_entry& entry,
                        const std::vector<Predicate>& preds, int depth) -> bool {
    for (const auto& p : preds) {
        switch (p.type) {
            case Predicate::Name: {
                auto fname = entry.path().filename().string();
                if (!glob_match(p.value, fname)) return false;
                break;
            }
            case Predicate::TypeMatch: {
                if (p.value.size() != 1) return false;
                char tc = file_type_char(entry);
                if (tc != p.value[0]) return false;
                break;
            }
            case Predicate::MaxDepth: {
                if (depth > p.numeric) return false;
                break;
            }
            case Predicate::Exec:
                break; // exec is handled in action, not filtering
        }
    }
    return true;
}

auto run_exec(const std::vector<std::string>& cmd_template,
              const std::string& filepath) -> void {
    std::vector<std::string> args;
    for (const auto& part : cmd_template) {
        if (part == "{}") {
            args.push_back(filepath);
        } else {
            args.push_back(part);
        }
    }
    // build argv
    std::vector<char*> argv_arr;
    for (auto& a : args) argv_arr.push_back(a.data());
    argv_arr.push_back(nullptr);

    // fork-like: just system() with proper quoting isn't great,
    // so use execvp via fork
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv_arr[0], argv_arr.data());
        _Exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

auto do_find(const std::filesystem::path& root, const std::vector<Predicate>& preds) -> int {
    bool has_exec = false;
    for (const auto& p : preds) {
        if (p.type == Predicate::Exec) { has_exec = true; break; }
    }

    std::error_code ec;
    auto it = std::filesystem::recursive_directory_iterator(root,
        std::filesystem::directory_options::follow_directory_symlink, ec);
    if (ec) {
        std::fprintf(stderr, "cfbox find: '%s': %s\n",
                     root.string().c_str(), ec.message().c_str());
        return 1;
    }

    int rc = 0;
    // Check the root itself
    {
        std::error_code ec2;
        std::filesystem::directory_entry root_entry(root, ec2);
        if (!ec2 && matches_predicates(root_entry, preds, 0)) {
            if (has_exec) {
                for (const auto& p : preds) {
                    if (p.type == Predicate::Exec) {
                        run_exec(p.exec_cmd, root_entry.path().string());
                    }
                }
            } else {
                std::printf("%s\n", root_entry.path().string().c_str());
            }
        }
    }

    for (const auto& entry : it) {
        int depth = it.depth() + 1; // +1 because root is depth 0

        // Check maxdepth predicate — disable recursion if we'd exceed it
        bool exceeded = false;
        for (const auto& p : preds) {
            if (p.type == Predicate::MaxDepth && depth > p.numeric) {
                exceeded = true;
                it.disable_recursion_pending();
                break;
            }
        }
        // Even at maxdepth+1 we might want to print the name
        // but standard find doesn't list beyond maxdepth, so skip
        if (exceeded) continue;

        if (matches_predicates(entry, preds, depth)) {
            if (has_exec) {
                for (const auto& p : preds) {
                    if (p.type == Predicate::Exec) {
                        run_exec(p.exec_cmd, entry.path().string());
                    }
                }
            } else {
                std::printf("%s\n", entry.path().string().c_str());
            }
        }
    }

    return rc;
}

} // namespace

auto find_main(int argc, char* argv[]) -> int {
    if (argc < 2) {
        // default: find .
        return do_find(".", {});
    }

    // First non-option argument is the path (if it doesn't start with '-')
    // Otherwise default to "."
    int start = 1;
    std::filesystem::path root = ".";

    if (argv[1][0] != '-') {
        root = argv[1];
        start = 2;
    }

    auto preds = parse_predicates(argc, argv, start);

    return do_find(root, preds);
}
