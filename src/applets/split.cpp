#include <cstdio>
#include <cstring>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

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
        lines = std::stol(std::string{*l});
    }
    if (auto b = parsed.get_any('b', "bytes")) {
        bytes = std::stol(std::string{*b});
    }
    bool numeric = parsed.has('d');

    const auto& pos = parsed.positional();
    std::string input_path = pos.empty() ? "-" : std::string{pos[0]};
    std::string prefix = (pos.size() > 1) ? std::string{pos[1]} : "x";

    auto data_result = (input_path == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(input_path);
    if (!data_result) {
        std::fprintf(stderr, "cfbox split: %s\n", data_result.error().msg.c_str());
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
                std::fprintf(stderr, "cfbox split: %s\n", wresult.error().msg.c_str());
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
                                std::fprintf(stderr, "cfbox split: %s\n", wresult.error().msg.c_str());
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
