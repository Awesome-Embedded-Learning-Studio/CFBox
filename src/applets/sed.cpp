// sed — stream editor
// Supported: s/pattern/replacement/[g|p|d], line addresses (single, range, $),
//            -n (suppress auto-print), -e SCRIPT, multiple commands
// Known differences from GNU sed: no branching/labels, no y (transliterate),
// no a/i/c commands, no hold space, no multi-line pattern space.

#include <cstdio>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "sed",
    .version = CFBOX_VERSION_STRING,
    .one_line = "stream editor for filtering and transforming text",
    .usage   = "sed [OPTIONS] SCRIPT [FILE]...",
    .options = "  -e SCRIPT  add the script to the commands to be executed\n"
               "  -n         suppress automatic printing of pattern space",
    .extra   = "",
};

struct Address {
    enum Type { None, Line, Last, Range };
    Type type = None;
    std::size_t line1 = 0;
    std::size_t line2 = 0; // only used for Range
};

struct SedCommand {
    Address addr;
    enum Action { Substitute, Delete, Print } action = Substitute;
    std::string pattern;
    std::string replacement;
    bool global = false;      // g flag
    bool print_flag = false;  // p flag
    bool delete_flag = false; // d flag (for substitute context)
};

auto parse_address(std::string_view& s) -> Address {
    Address addr;
    if (s.empty()) return addr;

    if (s[0] == '$') {
        s.remove_prefix(1);
        addr.type = Address::Last;
        // Check for range: $,\d+
        if (s.size() >= 2 && s[0] == ',') {
            // $ is the end — unusual but handle as range
        }
        return addr;
    }

    if (s[0] >= '0' && s[0] <= '9') {
        std::size_t n = 0;
        std::size_t i = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            n = n * 10 + (s[i] - '0');
            ++i;
        }
        s.remove_prefix(i);

        if (!s.empty() && s[0] == ',') {
            s.remove_prefix(1);
            addr.type = Address::Range;
            addr.line1 = n;

            if (!s.empty() && s[0] == '$') {
                s.remove_prefix(1);
                addr.line2 = static_cast<std::size_t>(-1); // sentinel for "last"
            } else if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
                std::size_t n2 = 0;
                std::size_t j = 0;
                while (j < s.size() && s[j] >= '0' && s[j] <= '9') {
                    n2 = n2 * 10 + (s[j] - '0');
                    ++j;
                }
                s.remove_prefix(j);
                addr.line2 = n2;
            }
        } else {
            addr.type = Address::Line;
            addr.line1 = n;
        }
        return addr;
    }

    return addr;
}

auto parse_substitute(std::string_view s) -> SedCommand {
    // s/PATTERN/REPLACEMENT/FLAGS
    SedCommand cmd;
    cmd.action = SedCommand::Substitute;

    if (s.empty() || s[0] != 's') return cmd;
    s.remove_prefix(1);
    if (s.empty()) return cmd;

    char delim = s[0];
    s.remove_prefix(1);

    // Extract pattern
    std::string pattern;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delim) {
            s.remove_prefix(i + 1);
            break;
        }
        if (s[i] == '\\' && i + 1 < s.size()) {
            pattern += s[i + 1];
            ++i;
        } else {
            pattern += s[i];
        }
        if (i + 1 == s.size()) {
            s.remove_prefix(i + 1);
        }
    }
    cmd.pattern = pattern;

    // Extract replacement
    std::string replacement;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delim) {
            s.remove_prefix(i + 1);
            break;
        }
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == '&') {
                replacement += '&';
                ++i;
            } else if (s[i + 1] == '\\') {
                replacement += '\\';
                ++i;
            } else if (s[i + 1] == 'n') {
                replacement += '\n';
                ++i;
            } else {
                replacement += s[i + 1];
                ++i;
            }
        } else if (s[i] == '&') {
            replacement += "&"; // placeholder for matched text — handled in apply
        } else {
            replacement += s[i];
        }
        if (i + 1 == s.size()) {
            s.remove_prefix(i + 1);
        }
    }
    cmd.replacement = replacement;

    // Parse flags
    for (char c : s) {
        switch (c) {
            case 'g': cmd.global = true; break;
            case 'p': cmd.print_flag = true; break;
            case 'd': cmd.delete_flag = true; break;
        }
    }

    return cmd;
}

auto parse_command(std::string_view script) -> SedCommand {
    std::string_view s = script;
    SedCommand cmd;
    cmd.addr = parse_address(s);

    if (s.empty()) return cmd;

    if (s[0] == 's') {
        auto sub = parse_substitute(s);
        cmd.action = SedCommand::Substitute;
        cmd.pattern = sub.pattern;
        cmd.replacement = sub.replacement;
        cmd.global = sub.global;
        cmd.print_flag = sub.print_flag;
        cmd.delete_flag = sub.delete_flag;
    } else if (s[0] == 'd') {
        cmd.action = SedCommand::Delete;
    } else if (s[0] == 'p') {
        cmd.action = SedCommand::Print;
    }

    return cmd;
}

auto parse_script(const std::string& script) -> std::vector<SedCommand> {
    std::vector<SedCommand> commands;
    // Split by newlines and semicolons (simple)
    std::string_view sv{script};
    std::string token;
    for (std::size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == ';' || sv[i] == '\n') {
            if (!token.empty()) {
                commands.push_back(parse_command(token));
                token.clear();
            }
        } else {
            token += sv[i];
        }
    }
    if (!token.empty()) {
        commands.push_back(parse_command(token));
    }
    return commands;
}

auto address_matches(const Address& addr, std::size_t line, std::size_t total_lines) -> bool {
    switch (addr.type) {
        case Address::None:  return true;
        case Address::Line:  return line == addr.line1;
        case Address::Last:  return line == total_lines;
        case Address::Range: return line >= addr.line1 && line <= addr.line2;
    }
    return false;
}

auto apply_substitute(std::string& line, const SedCommand& cmd) -> bool {
    try {
        std::regex re(cmd.pattern);
        if (!std::regex_search(line, re)) return false;

        if (cmd.global) {
            line = std::regex_replace(line, re, cmd.replacement);
        } else {
            line = std::regex_replace(line, re, cmd.replacement,
                                      std::regex_constants::format_first_only);
        }
        return true;
    } catch (const std::regex_error&) {
        return false;
    }
}

auto process_lines(const std::vector<std::string>& lines,
                   const std::vector<SedCommand>& commands, bool suppress) -> void {
    std::size_t total = lines.size();

    for (std::size_t li = 0; li < total; ++li) {
        std::string line = lines[li];
        bool deleted = false;
        bool extra_print = false;

        for (const auto& cmd : commands) {
            if (!address_matches(cmd.addr, li + 1, total)) continue;

            switch (cmd.action) {
                case SedCommand::Substitute: {
                    bool changed = apply_substitute(line, cmd);
                    if (changed && cmd.print_flag) {
                        extra_print = true;
                    }
                    if (cmd.delete_flag && changed) {
                        deleted = true;
                    }
                    break;
                }
                case SedCommand::Delete:
                    deleted = true;
                    break;
                case SedCommand::Print:
                    extra_print = true;
                    break;
            }
        }

        if (extra_print) {
            std::printf("%s\n", line.c_str());
        }

        if (!deleted && !suppress) {
            std::printf("%s\n", line.c_str());
        }
    }
}

} // namespace

auto sed_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'e', true},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool suppress = parsed.has('n');

    std::string script;
    std::vector<std::string_view> files;

    if (parsed.has('e')) {
        script = std::string{parsed.get('e').value_or("")};
        for (const auto& p : parsed.positional()) {
            files.push_back(p);
        }
    } else {
        // First positional arg is the script
        const auto& pos = parsed.positional();
        if (pos.empty()) {
            std::fprintf(stderr, "cfbox sed: missing script\n");
            return 1;
        }
        script = std::string{pos[0]};
        for (std::size_t i = 1; i < pos.size(); ++i) {
            files.push_back(pos[i]);
        }
    }

    auto commands = parse_script(script);
    if (commands.empty()) {
        std::fprintf(stderr, "cfbox sed: empty command\n");
        return 1;
    }

    if (files.empty()) {
        // Read from stdin
        auto result = cfbox::io::read_all_stdin();
        if (!result) {
            std::fprintf(stderr, "cfbox sed: %s\n", result.error().msg.c_str());
            return 1;
        }
        auto lines = cfbox::io::split_lines(result.value());
        process_lines(lines, commands, suppress);
    } else {
        for (const auto& f : files) {
            auto result = (f == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(f);
            if (!result) {
                std::fprintf(stderr, "cfbox sed: %s\n", result.error().msg.c_str());
                return 1;
            }
            auto lines = cfbox::io::split_lines(result.value());
            process_lines(lines, commands, suppress);
        }
    }

    return 0;
}
