#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cfbox::deflate {

struct HuffEntry {
    std::uint16_t symbol;
    std::uint8_t bits;
};

class BitReader {
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    std::uint32_t buf_ = 0;
    int buf_bits_ = 0;

    auto fill(int need) -> void {
        while (buf_bits_ < need && pos_ < size_) {
            buf_ |= static_cast<std::uint32_t>(data_[pos_]) << buf_bits_;
            buf_bits_ += 8;
            ++pos_;
        }
    }

public:
    BitReader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size) {}

    auto read(int n) -> std::uint32_t {
        fill(n);
        auto r = buf_ & ((1u << n) - 1);
        buf_ >>= n;
        buf_bits_ -= n;
        return r;
    }

    auto peek(int n) -> std::uint32_t {
        fill(n);
        return buf_ & ((1u << n) - 1);
    }

    auto skip(int n) -> void {
        buf_ >>= n;
        buf_bits_ -= n;
    }

    auto align() -> void {
        auto discard = static_cast<int>(buf_bits_ % 8);
        if (discard > 0) { buf_ >>= discard; buf_bits_ -= discard; }
    }

    auto read_block(std::size_t n, std::uint8_t* out) -> bool {
        while (buf_bits_ >= 8 && n > 0) {
            *out++ = static_cast<std::uint8_t>(buf_ & 0xFF);
            buf_ >>= 8;
            buf_bits_ -= 8;
            --n;
        }
        buf_bits_ = 0;
        buf_ = 0;
        if (pos_ + n > size_) return false;
        std::memcpy(out, data_ + pos_, n);
        pos_ += n;
        return true;
    }
};

inline auto build_huffman_table(const std::vector<int>& lengths, int max_bits)
    -> std::vector<HuffEntry> {
    auto sz = static_cast<std::size_t>(1 << max_bits);
    std::vector<HuffEntry> table(sz, {0, 0});

    std::vector<int> bl_count(max_bits + 1, 0);
    for (auto l : lengths) if (l > 0) bl_count[l]++;

    std::vector<int> next_code(max_bits + 1, 0);
    int code = 0;
    for (int b = 1; b <= max_bits; ++b) {
        code = (code + bl_count[b - 1]) << 1;
        next_code[b] = code;
    }

    for (std::size_t sym = 0; sym < lengths.size(); ++sym) {
        int len = lengths[sym];
        if (len == 0) continue;
        int c = next_code[len]++;
        std::uint32_t rev = 0;
        for (int i = 0; i < len; ++i)
            rev |= static_cast<std::uint32_t>(((c >> i) & 1) << (len - 1 - i));
        auto step = static_cast<std::size_t>(1 << len);
        for (std::size_t idx = rev; idx < sz; idx += step)
            table[idx] = {static_cast<std::uint16_t>(sym), static_cast<std::uint8_t>(len)};
    }
    return table;
}

inline auto decode_symbol(BitReader& br, const std::vector<HuffEntry>& table, int max_bits)
    -> int {
    auto peek = br.peek(max_bits);
    auto& e = table[peek];
    br.skip(e.bits);
    return e.symbol;
}

static constexpr int length_base[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static constexpr int length_extra[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static constexpr int dist_base[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};
static constexpr int dist_extra[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

inline auto fixed_lit_lengths() -> std::vector<int> {
    std::vector<int> l(288);
    for (int i = 0; i <= 143; ++i) l[static_cast<std::size_t>(i)] = 8;
    for (int i = 144; i <= 255; ++i) l[static_cast<std::size_t>(i)] = 9;
    for (int i = 256; i <= 279; ++i) l[static_cast<std::size_t>(i)] = 7;
    for (int i = 280; i <= 287; ++i) l[static_cast<std::size_t>(i)] = 8;
    return l;
}

inline auto fixed_dist_lengths() -> std::vector<int> {
    return std::vector<int>(32, 5);
}

inline auto decode_dynamic_tables(BitReader& br,
    std::vector<HuffEntry>& lit_table, int& lit_bits,
    std::vector<HuffEntry>& dist_table, int& dist_bits) -> bool {
    auto hlit = static_cast<std::size_t>(br.read(5)) + 257;
    auto hdist = static_cast<std::size_t>(br.read(5)) + 1;
    int hclen = static_cast<int>(br.read(4)) + 4;

    static constexpr int cl_order[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    std::vector<int> cl_lengths(19, 0);
    for (int i = 0; i < hclen; ++i)
        cl_lengths[static_cast<std::size_t>(cl_order[i])] = static_cast<int>(br.read(3));

    auto cl_table = build_huffman_table(cl_lengths, 7);

    auto total = hlit + hdist;
    std::vector<int> all(total, 0);
    std::size_t i = 0;
    while (i < total) {
        int sym = decode_symbol(br, cl_table, 7);
        if (sym < 16) {
            all[i++] = sym;
        } else if (sym == 16) {
            int rep = static_cast<int>(br.read(2)) + 3;
            int prev = (i > 0) ? all[i - 1] : 0;
            for (int j = 0; j < rep && i < total; ++j) all[i++] = prev;
        } else if (sym == 17) {
            int rep = static_cast<int>(br.read(3)) + 3;
            for (int j = 0; j < rep && i < total; ++j) all[i++] = 0;
        } else if (sym == 18) {
            int rep = static_cast<int>(br.read(7)) + 11;
            for (int j = 0; j < rep && i < total; ++j) all[i++] = 0;
        } else {
            return false;
        }
    }

    std::vector<int> ll(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(hlit));
    std::vector<int> dl(all.begin() + static_cast<std::ptrdiff_t>(hlit), all.end());

    lit_bits = 1;
    for (auto v : ll) lit_bits = std::max(lit_bits, v);
    dist_bits = 1;
    for (auto v : dl) dist_bits = std::max(dist_bits, v);

    lit_table = build_huffman_table(ll, lit_bits);
    dist_table = build_huffman_table(dl, dist_bits);
    return true;
}

inline auto inflate(const std::uint8_t* data, std::size_t size, std::size_t expected = 0)
    -> std::string {
    BitReader br(data, size);
    std::string out;
    if (expected > 0) out.reserve(expected);

    bool done = false;
    while (!done) {
        done = br.read(1) != 0;
        int btype = static_cast<int>(br.read(2));

        if (btype == 0) {
            br.align();
            std::uint8_t hdr[4];
            if (!br.read_block(4, hdr)) break;
            auto len = static_cast<std::size_t>(hdr[0]) |
                       (static_cast<std::size_t>(hdr[1]) << 8);
            std::string blk(len, '\0');
            if (!br.read_block(len, reinterpret_cast<std::uint8_t*>(blk.data()))) break;
            out += blk;
        } else if (btype == 1 || btype == 2) {
            std::vector<HuffEntry> lt, dt;
            int lb, db;

            if (btype == 1) {
                lt = build_huffman_table(fixed_lit_lengths(), 9);
                dt = build_huffman_table(fixed_dist_lengths(), 5);
                lb = 9; db = 5;
            } else {
                if (!decode_dynamic_tables(br, lt, lb, dt, db)) break;
            }

            for (;;) {
                int sym = decode_symbol(br, lt, lb);
                if (sym < 256) {
                    out += static_cast<char>(sym);
                } else if (sym == 256) {
                    break;
                } else {
                    int li = sym - 257;
                    int length = length_base[li];
                    if (length_extra[li] > 0)
                        length += static_cast<int>(br.read(length_extra[li]));

                    int ds = decode_symbol(br, dt, db);
                    int dist = dist_base[ds];
                    if (dist_extra[ds] > 0)
                        dist += static_cast<int>(br.read(dist_extra[ds]));

                    auto src = out.size() - static_cast<std::size_t>(dist);
                    for (int j = 0; j < length; ++j)
                        out += out[src + static_cast<std::size_t>(j)];
                }
            }
        } else {
            break;
        }
    }
    return out;
}

} // namespace cfbox::deflate
