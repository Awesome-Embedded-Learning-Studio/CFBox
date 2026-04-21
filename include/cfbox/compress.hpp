#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <zlib.h>

namespace cfbox::compress {

inline auto gzip_compress(std::string_view data) -> std::string {
    z_stream strm{};
    deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);

    std::string output;
    output.resize(data.size() + data.size() / 10 + 256);

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_out = reinterpret_cast<Bytef*>(output.data());
    strm.avail_out = static_cast<uInt>(output.size());

    deflate(&strm, Z_FINISH);
    output.resize(strm.total_out);
    deflateEnd(&strm);
    return output;
}

inline auto gzip_decompress(std::string_view data) -> std::string {
    z_stream strm{};
    inflateInit2(&strm, 15 + 16);

    std::string output;
    output.resize(data.size() * 4 + 4096);

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));
    strm.avail_in = static_cast<uInt>(data.size());

    int ret;
    do {
        if (output.size() - strm.total_out < 4096) {
            output.resize(output.size() * 2);
        }
        strm.next_out = reinterpret_cast<Bytef*>(output.data() + strm.total_out);
        strm.avail_out = static_cast<uInt>(output.size() - strm.total_out);
        ret = inflate(&strm, Z_NO_FLUSH);
    } while (ret == Z_OK);

    output.resize(strm.total_out);
    inflateEnd(&strm);
    return output;
}

} // namespace cfbox::compress
