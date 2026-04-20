#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/escape.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "printf",
    .version = CFBOX_VERSION_STRING,
    .one_line = "format and print data",
    .usage   = "printf FORMAT [ARG]...",
    .options = "  FORMAT  printf format string",
    .extra   = "",
};

// Extract a full format spec: %[flags][width][.precision]specifier
// Returns (format_str, arg_consumed) where format_str is like "%.2f"
auto extract_spec(std::string_view fmt, std::size_t pos)
    -> std::pair<std::string, bool> {
    if (pos >= fmt.size() || fmt[pos] != '%') return {"%", false};
    std::size_t i = pos + 1;

    // flags: -, +, 0, space, #
    while (i < fmt.size() && (fmt[i] == '-' || fmt[i] == '+' ||
           fmt[i] == '0' || fmt[i] == ' ' || fmt[i] == '#')) {
        ++i;
    }
    // width: digits or *
    while (i < fmt.size() && fmt[i] >= '0' && fmt[i] <= '9') ++i;
    // precision: . followed by digits or *
    if (i < fmt.size() && fmt[i] == '.') {
        ++i;
        while (i < fmt.size() && fmt[i] >= '0' && fmt[i] <= '9') ++i;
    }
    // specifier
    if (i >= fmt.size()) return {std::string{fmt.substr(pos)}, false};

    char spec = fmt[i];
    if (spec == '%') {
        return {std::string{fmt.substr(pos, i - pos + 1)}, false};
    }
    return {std::string{fmt.substr(pos, i - pos + 1)}, true};
}

auto format_with_spec(const std::string& spec, std::string_view arg) -> std::string {
    if (spec.size() < 2) return spec;
    char conv = spec.back();
    char buf[256];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    switch (conv) {
    case 's':
        std::snprintf(buf, sizeof(buf), spec.c_str(), std::string{arg}.c_str());
        return buf;
    case 'd':
    case 'i':
        std::snprintf(buf, sizeof(buf), spec.c_str(),
            static_cast<int>(std::strtol(std::string{arg}.c_str(), nullptr, 10)));
        return buf;
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 'e':
    case 'E':
        std::snprintf(buf, sizeof(buf), spec.c_str(),
            std::strtod(std::string{arg}.c_str(), nullptr));
        return buf;
#pragma GCC diagnostic pop
    case 'c':
        return std::string(1, arg.empty() ? '\0' : arg[0]);
    case '%':
        return "%";
    default:
        return spec;
    }
}

auto expand_format(std::string_view fmt, const std::vector<std::string_view>& args,
                   std::size_t& arg_idx) -> std::string {
    std::string out;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '%') {
            auto [spec, consumes] = extract_spec(fmt, i);
            i += spec.size() - 1;

            if (!consumes) {
                if (spec == "%%") { out += '%'; }
                else { out += spec; }
                continue;
            }

            auto arg_val = (arg_idx < args.size()) ? args[arg_idx++] : "";
            out += format_with_spec(spec, arg_val);
        } else if (fmt[i] == '\\') {
            if (i + 1 < fmt.size()) {
                switch (fmt[i + 1]) {
                case 'n':  out += '\n'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'a':  out += '\a'; ++i; break;
                case 'b':  out += '\b'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                default: out += fmt[i]; break;
                }
            } else {
                out += '\\';
            }
        } else {
            out += fmt[i];
        }
    }
    return out;
}

auto count_specs(std::string_view fmt) -> std::size_t {
    std::size_t n = 0;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '%') {
            auto [spec, consumes] = extract_spec(fmt, i);
            if (consumes) ++n;
            i += spec.size() - 1;
        }
    }
    return n;
}

} // namespace

auto printf_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) return 0;

    std::string_view fmt = pos[0];
    std::size_t specs_per_cycle = count_specs(fmt);

    if (specs_per_cycle == 0) {
        // Still handle %% and escapes, but no arg substitution
        std::vector<std::string_view> no_args;
        std::size_t dummy = 0;
        std::fputs(expand_format(fmt, no_args, dummy).c_str(), stdout);
        return 0;
    }

    std::vector<std::string_view> args(pos.begin() + 1, pos.end());
    std::size_t arg_idx = 0;

    if (args.empty()) {
        std::fputs(expand_format(fmt, args, arg_idx).c_str(), stdout);
        return 0;
    }

    // Reuse format string while args remain
    do {
        std::size_t start = arg_idx;
        std::fputs(expand_format(fmt, args, arg_idx).c_str(), stdout);
        if (arg_idx == start) break;
    } while (arg_idx < args.size());

    return 0;
}
