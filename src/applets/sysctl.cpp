#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <filesystem>

#include <cfbox/applet.hpp>
#include <cfbox/help.hpp>
#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/io.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "sysctl",
    .version = CFBOX_VERSION_STRING,
    .one_line = "configure kernel parameters at runtime",
    .usage   = "sysctl [-a] [-n] [-w KEY=VALUE] [-p FILE] [KEY]",
    .options = "  -a     display all values\n"
               "  -n     show only values (no keys)\n"
               "  -w     set a value\n"
               "  -p     load values from file",
    .extra   = "",
};

auto key_to_path(std::string_view key) -> std::string {
    std::string path = "/proc/sys/";
    for (auto c : key) {
        if (c == '.') path += '/';
        else path += c;
    }
    return path;
}

auto path_to_key(std::string_view path) -> std::string {
    // Strip /proc/sys/ prefix and convert / to .
    const char* prefix = "/proc/sys/";
    if (path.size() > 10 && path.substr(0, 10) == prefix)
        path = path.substr(10);
    std::string key;
    for (auto c : path) {
        if (c == '/') key += '.';
        else key += c;
    }
    return key;
}

auto read_sysctl_value(const std::string& path) -> std::string {
    // /proc/sys/* values are single-line; emulate the original fgets-first-line
    // behavior via read_all (handles /proc 0-size fallback) then take up to the
    // first newline, trimming trailing CR/LF. Open errors / empty reads map to
    // the original empty-string "skip" result.
    auto content = cfbox::io::read_all(path);
    if (!content || content->empty()) return {};
    auto end = content->find('\n');
    auto first = (end == std::string::npos) ? *content : content->substr(0, end);
    auto len = first.size();
    while (len > 0 && (first[len - 1] == '\n' || first[len - 1] == '\r')) {
        first[--len] = '\0';
    }
    return first;
}

auto write_sysctl_value(const std::string& path, std::string_view value) -> bool {
    auto f = cfbox::io::open_file(path, "w");
    if (!f) return false;
    std::fprintf(f->get(), "%.*s\n", static_cast<int>(value.size()), value.data());
    // Explicit close to capture the flush/write status (fclose==0) the original
    // returned; RAII would otherwise close silently on destruction.
    return std::fclose(f->release()) == 0;
}

auto show_key(std::string_view key, bool no_name) -> bool {
    auto path = key_to_path(key);
    auto val = read_sysctl_value(path);
    if (val.empty()) return false;
    if (no_name) std::printf("%s\n", val.c_str());
    else std::printf("%.*s = %s\n", static_cast<int>(key.size()), key.data(), val.c_str());
    return true;
}

auto show_all(bool no_name) -> void {
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator("/proc/sys", ec)) {
        if (!entry.is_regular_file()) continue;
        auto key = path_to_key(entry.path().string());
        auto val = read_sysctl_value(entry.path().string());
        if (val.empty()) continue;
        if (no_name) std::printf("%s\n", val.c_str());
        else std::printf("%s = %s\n", key.c_str(), val.c_str());
    }
}

auto load_file(const std::string& filepath, bool no_name) -> int {
    auto f = cfbox::io::open_file(filepath, "r");
    if (!f) {
        CFBOX_ERR("sysctl", "cannot open %s", filepath.c_str());
        return 1;
    }

    int errors = 0;
    auto result = cfbox::io::for_each_line(f->get(), [&](const std::string& line) {
        // for_each_line strips the trailing '\n'; also drop a trailing '\r'
        // (CRLF) to match the original fgets-based trimming.
        std::string trimmed = line;
        while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
            trimmed.pop_back();
        }
        auto len = trimmed.size();
        // Skip comments and empty lines
        if (len == 0 || trimmed[0] == '#' || trimmed[0] == ';') return;
        auto* eq = std::strchr(trimmed.c_str(), '=');
        if (!eq) return;
        std::string key(trimmed.c_str(), eq);
        std::string val(eq + 1);
        // Trim whitespace
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());

        auto path = key_to_path(key);
        if (!write_sysctl_value(path, val)) {
            CFBOX_ERR("sysctl", "cannot set %s", key.c_str());
            ++errors;
        } else if (!no_name) {
            std::printf("%s = %s\n", key.c_str(), val.c_str());
        }
    });
    (void)result; // line-level I/O errors leave the per-line error count as-is
    return errors > 0 ? 1 : 0;
}

} // anonymous namespace

auto sysctl_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'a', false, "all"},
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'w', false},
        cfbox::args::OptSpec{'e', false},
        cfbox::args::OptSpec{'p', true, "load"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool no_name = parsed.has('n');
    bool do_write = parsed.has('w');

    if (parsed.has('a') || parsed.has_long("all")) {
        show_all(no_name);
        return 0;
    }

    if (auto file = parsed.get_any('p', "load")) {
        return load_file(std::string(*file), no_name);
    }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        // Default: show all (like sysctl without args on some systems)
        show_all(no_name);
        return 0;
    }

    int errors = 0;
    for (const auto& arg : pos) {
        auto s = std::string(arg);
        if (do_write) {
            auto eq = s.find('=');
            if (eq == std::string::npos) {
                CFBOX_ERR("sysctl", "invalid setting: %s", s.c_str());
                ++errors;
                continue;
            }
            auto key = s.substr(0, eq);
            auto val = s.substr(eq + 1);
            auto path = key_to_path(key);
            if (!write_sysctl_value(path, val)) {
                CFBOX_ERR("sysctl", "cannot set %s", key.c_str());
                ++errors;
            } else if (!no_name) {
                std::printf("%s = %s\n", key.c_str(), val.c_str());
            }
        } else {
            if (!show_key(s, no_name)) {
                CFBOX_ERR("sysctl", "cannot stat %s", s.c_str());
                ++errors;
            }
        }
    }

    return errors > 0 ? 1 : 0;
}
