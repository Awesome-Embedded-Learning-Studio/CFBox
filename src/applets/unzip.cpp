#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

#include <zlib.h>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "unzip",
    .version = CFBOX_VERSION_STRING,
    .one_line = "list, test and extract compressed files in a ZIP archive",
    .usage   = "unzip [-l] [-d DIR] FILE",
    .options = "  -l     list archive contents\n"
               "  -d DIR extract into DIR",
    .extra   = "",
};

struct ZipEntry {
    std::string name;
    unsigned long comp_size;
    unsigned long uncomp_size;
    unsigned long offset;
    unsigned int method;
};

static auto read_le16(const unsigned char* p) -> unsigned int {
    return p[0] | (p[1] << 8);
}

static auto read_le32(const unsigned char* p) -> unsigned long {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<unsigned long>(p[3]) << 24);
}
} // namespace

auto unzip_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'l', false},
        cfbox::args::OptSpec{'d', true, "directory"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool list_mode = parsed.has('l');
    std::string outdir = ".";
    if (auto d = parsed.get_any('d', "directory")) outdir = std::string{*d};

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox unzip: missing archive\n");
        return 1;
    }

    auto input = cfbox::io::read_all(std::string{pos[0]});
    if (!input) {
        std::fprintf(stderr, "cfbox unzip: %s\n", input.error().msg.c_str());
        return 1;
    }
    const auto& data = *input;

    // Find End of Central Directory
    std::size_t eocd = data.size();
    for (std::size_t i = data.size(); i >= 22; --i) {
        if (data[i - 22] == 'P' && data[i - 21] == 'K' &&
            data[i - 20] == 0x05 && data[i - 19] == 0x06) {
            eocd = i - 22;
            break;
        }
    }
    if (eocd >= data.size()) {
        std::fprintf(stderr, "cfbox unzip: not a valid zip file\n");
        return 1;
    }

    auto cd_offset = read_le32(reinterpret_cast<const unsigned char*>(data.data() + eocd + 16));
    auto cd_entries = read_le16(reinterpret_cast<const unsigned char*>(data.data() + eocd + 10));

    // Parse central directory
    std::vector<ZipEntry> entries;
    std::size_t off = cd_offset;
    for (unsigned i = 0; i < cd_entries && off + 46 <= data.size(); ++i) {
        if (data[off] != 'P' || data[off+1] != 'K' || data[off+2] != 0x01 || data[off+3] != 0x02) break;
        auto name_len = read_le16(reinterpret_cast<const unsigned char*>(data.data() + off + 28));
        auto method = read_le16(reinterpret_cast<const unsigned char*>(data.data() + off + 10));
        auto comp_sz = read_le32(reinterpret_cast<const unsigned char*>(data.data() + off + 20));
        auto uncomp_sz = read_le32(reinterpret_cast<const unsigned char*>(data.data() + off + 24));
        auto local_off = read_le32(reinterpret_cast<const unsigned char*>(data.data() + off + 42));
        std::string name{data.substr(off + 46, name_len)};

        entries.push_back({name, comp_sz, uncomp_sz, local_off, method});
        auto extra_len = read_le16(reinterpret_cast<const unsigned char*>(data.data() + off + 30));
        auto comment_len = read_le16(reinterpret_cast<const unsigned char*>(data.data() + off + 32));
        off += 46 + name_len + extra_len + comment_len;
    }

    if (list_mode) {
        std::printf("  Length      Date    Time    Name\n");
        std::printf("---------  ---------- -----   ----\n");
        unsigned long total = 0;
        for (const auto& e : entries) {
            std::printf("%9lu                   %s\n", e.uncomp_size, e.name.c_str());
            total += e.uncomp_size;
        }
        std::printf("---------                    -------\n");
        std::printf("%9lu                   %zu files\n", total, entries.size());
        return 0;
    }

    // Extract
    for (const auto& e : entries) {
        if (e.name.back() == '/') continue; // directory entry
        auto local = e.offset;
        if (local + 30 > data.size()) continue;
        auto lname = read_le16(reinterpret_cast<const unsigned char*>(data.data() + local + 26));
        auto lextra = read_le16(reinterpret_cast<const unsigned char*>(data.data() + local + 28));
        auto data_start = local + 30 + lname + lextra;
        auto compressed = data.substr(data_start, e.comp_size);

        std::string content;
        if (e.method == 0) {
            content = std::string{compressed};
        } else if (e.method == 8) {
            content.resize(e.uncomp_size);
            z_stream strm{};
            inflateInit2(&strm, -15);
            strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
            strm.avail_in = static_cast<uInt>(compressed.size());
            strm.next_out = reinterpret_cast<Bytef*>(content.data());
            strm.avail_out = static_cast<uInt>(content.size());
            inflate(&strm, Z_FINISH);
            inflateEnd(&strm);
        }

        auto outpath = outdir + "/" + e.name;
        // Create parent dirs
        auto last_slash = outpath.rfind('/');
        if (last_slash != std::string::npos) {
            std::filesystem::create_directories(outpath.substr(0, last_slash));
        }
        auto wresult = cfbox::io::write_all(outpath, content);
        if (!wresult) {
            std::fprintf(stderr, "cfbox unzip: %s\n", wresult.error().msg.c_str());
        }
    }
    return 0;
}
