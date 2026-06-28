#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/error.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "split",
    .version = CFBOX_VERSION_STRING,
    .one_line = "split a file into pieces",
    .usage   = "split [-l LINES] [-b BYTES] [INPUT [PREFIX]]",
    .options = "  -l LINES  put LINES lines per output file (default 1000)\n"
               "  -b BYTES  put BYTES bytes per output file\n"
               "  -d        use numeric suffixes instead of alphabetic",
    .extra   = "",
};

static auto next_alpha_suffix(int n) -> std::string {
    std::string result;
    result += static_cast<char>('a' + (n / 26) % 26);
    result += static_cast<char>('a' + n % 26);
    return result;
}

static auto next_digit_suffix(int n) -> std::string {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d", n);
    return buf;
}

// Parse s as base-10 long with no throw. -l/-b historically went through std::stol
// (long range), so we keep long semantics here rather than cfbox::args::parse_int,
// which is int-bounded and would reject values > INT_MAX (e.g. split -b on multi-GB
// inputs). Matches parse_int's error shape so CFBOX_ERR output stays identical.
static auto parse_long(std::string_view s) -> cfbox::base::Result<long> {
    long v = 0;
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc{} || res.ptr != s.data() + s.size()) {
        return std::unexpected(cfbox::base::Error{EINVAL, "not a valid integer: '" + std::string{s} + '\''});
    }
    return v;
}
} // namespace

auto split_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'l', true, "lines"},
        cfbox::args::OptSpec{'b', true, "bytes"},
        cfbox::args::OptSpec{'d', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    long lines = 1000;
    long bytes = 0;
    if (auto l = parsed.get_any('l', "lines")) {
        auto parsed_lines = parse_long(*l);
        if (!parsed_lines) {
            CFBOX_ERR("split", "%s", parsed_lines.error().msg.c_str());
            return 2;
        }
        lines = *parsed_lines;
    }
    if (auto b = parsed.get_any('b', "bytes")) {
        auto parsed_bytes = parse_long(*b);
        if (!parsed_bytes) {
            CFBOX_ERR("split", "%s", parsed_bytes.error().msg.c_str());
            return 2;
        }
        bytes = *parsed_bytes;
    }
    bool numeric = parsed.has('d');

    const auto& pos = parsed.positional();
    std::string input_path = pos.empty() ? "-" : std::string{pos[0]};
    std::string prefix = (pos.size() > 1) ? std::string{pos[1]} : "x";

    auto data_result = (input_path == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(input_path);
    if (!data_result) {
        CFBOX_ERR("split", "%s", data_result.error().msg.c_str());
        return 1;
    }

    const auto& data = *data_result;
    int file_num = 0;

    if (bytes > 0) {
        for (std::size_t offset = 0; offset < data.size(); offset += static_cast<std::size_t>(bytes)) {
            auto suffix = numeric ? next_digit_suffix(file_num) : next_alpha_suffix(file_num);
            auto fname = prefix + suffix;
            auto len = static_cast<std::size_t>(bytes);
            if (offset + len > data.size()) len = data.size() - offset;
            auto wresult = cfbox::io::write_all(fname, std::string_view{data.data() + offset, len});
            if (!wresult) {
                CFBOX_ERR("split", "%s", wresult.error().msg.c_str());
                return 1;
            }
            ++file_num;
        }
    } else {
        std::size_t line_start = 0;
        long line_count = 0;
        for (std::size_t i = 0; i <= data.size(); ++i) {
            if (i == data.size() || data[i] == '\n') {
                ++line_count;
                if (line_count >= lines || i == data.size()) {
                    if (line_start < i || (line_count > 0 && line_count >= lines)) {
                        auto suffix = numeric ? next_digit_suffix(file_num) : next_alpha_suffix(file_num);
                        auto fname = prefix + suffix;
                        auto end = i < data.size() ? i + 1 : i;
                        auto len = end - line_start;
                        if (len > 0) {
                            auto wresult = cfbox::io::write_all(fname,
                                std::string_view{data.data() + line_start, len});
                            if (!wresult) {
                                CFBOX_ERR("split", "%s", wresult.error().msg.c_str());
                                return 1;
                            }
                        }
                        ++file_num;
                        line_start = end;
                        line_count = 0;
                    }
                }
            }
        }
    }
    return 0;
}
