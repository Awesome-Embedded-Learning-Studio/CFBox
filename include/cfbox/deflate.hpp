#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cfbox::deflate {

class BitWriter {
    std::vector<std::uint8_t>& out_;
    int bit_pos_ = 0;
    std::uint8_t current_ = 0;

public:
    explicit BitWriter(std::vector<std::uint8_t>& out) : out_(out) {}
    ~BitWriter() { flush(); }

    // Write bits LSB-first (for non-Huffman data)
    auto write(std::uint32_t value, int nbits) -> void {
        for (int i = 0; i < nbits; ++i) {
            if ((value >> i) & 1)
                current_ |= static_cast<std::uint8_t>(1 << bit_pos_);
            ++bit_pos_;
            if (bit_pos_ == 8) {
                out_.push_back(current_);
                current_ = 0;
                bit_pos_ = 0;
            }
        }
    }

    // Write Huffman code MSB-first (RFC 1951: codes packed MSB-first)
    auto write_huffman(std::uint32_t code, int nbits) -> void {
        for (int i = nbits - 1; i >= 0; --i) {
            if ((code >> i) & 1)
                current_ |= static_cast<std::uint8_t>(1 << bit_pos_);
            ++bit_pos_;
            if (bit_pos_ == 8) {
                out_.push_back(current_);
                current_ = 0;
                bit_pos_ = 0;
            }
        }
    }

    auto flush() -> void {
        if (bit_pos_ > 0) {
            out_.push_back(current_);
            current_ = 0;
            bit_pos_ = 0;
        }
    }
};

// Fixed Huffman literal/length encoding (RFC 1951)
inline auto encode_fixed_lit(std::uint16_t sym, BitWriter& bw) -> void {
    if (sym <= 143) {
        bw.write_huffman(0x30u + sym, 8);
    } else if (sym <= 255) {
        bw.write_huffman(0x190u + sym - 144, 9);
    } else if (sym <= 279) {
        bw.write_huffman(static_cast<std::uint32_t>(sym - 256), 7);
    } else {
        bw.write_huffman(0xC0u + sym - 280, 8);
    }
}

// Fixed Huffman distance encoding (5 bits, MSB-first)
inline auto encode_fixed_dist(std::uint8_t dist_code, BitWriter& bw) -> void {
    bw.write_huffman(static_cast<std::uint32_t>(dist_code), 5);
}

static constexpr int len_base[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static constexpr int len_extra[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static constexpr int dst_base[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577
};
static constexpr int dst_extra[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

inline auto find_length_code(int length) -> int {
    for (int i = 28; i >= 0; --i)
        if (length >= len_base[i]) return i;
    return 0;
}

inline auto find_dist_code(int dist) -> int {
    for (int i = 29; i >= 0; --i)
        if (dist >= dst_base[i]) return i;
    return 0;
}

// LZ77 hash chain matcher
class Matcher {
    static constexpr int HASH_BITS = 15;
    static constexpr auto HASH_SIZE = std::size_t{1} << HASH_BITS;
    static constexpr int MAX_MATCH = 258;
    static constexpr int MIN_MATCH = 3;
    static constexpr int MAX_CHAIN = 128;

    std::vector<int> head_;
    std::vector<int> prev_;
    const std::uint8_t* data_;
    std::size_t size_;

    auto hash3(std::size_t pos) const -> std::size_t {
        auto a = static_cast<unsigned>(data_[pos]);
        auto b = static_cast<unsigned>(data_[pos + 1]);
        auto c = static_cast<unsigned>(data_[pos + 2]);
        return (static_cast<std::size_t>(a) << HASH_BITS ^
                static_cast<std::size_t>(b) << (HASH_BITS - 5) ^
                c) & (HASH_SIZE - 1);
    }

public:
    Matcher(const std::uint8_t* data, std::size_t size)
        : head_(HASH_SIZE, -1), prev_(size, -1), data_(data), size_(size) {}

    struct Match { int length; int distance; };

    auto find(std::size_t pos) -> Match {
        Match best{0, 0};
        if (pos + MIN_MATCH > size_) return best;

        auto h = hash3(pos);
        int chain = head_[h];
        int tries = MAX_CHAIN;

        while (chain >= 0 && tries-- > 0) {
            auto dist = static_cast<int>(pos - static_cast<std::size_t>(chain));
            if (dist > 32768) break;

            int len = 0;
            auto max_len = static_cast<int>(std::min(
                static_cast<std::size_t>(MAX_MATCH), size_ - pos));
            while (len < max_len && data_[static_cast<std::size_t>(chain) + static_cast<std::size_t>(len)] == data_[pos + static_cast<std::size_t>(len)])
                ++len;

            if (len >= MIN_MATCH && len > best.length) {
                best = {len, dist};
                if (len == MAX_MATCH) break;
            }
            chain = prev_[static_cast<std::size_t>(chain)];
        }

        prev_[pos] = head_[h];
        head_[h] = static_cast<int>(pos);
        return best;
    }
};

inline auto deflate_compress(const std::uint8_t* data, std::size_t size)
    -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> output;
    BitWriter bw(output);

    // BFINAL=1, BTYPE=01 (fixed Huffman)
    bw.write(1, 1);
    bw.write(1, 2);

    if (size == 0) {
        encode_fixed_lit(256, bw);
        bw.flush();
        return output;
    }

    Matcher matcher(data, size);
    std::size_t pos = 0;

    while (pos < size) {
        auto match = matcher.find(pos);

        if (match.length >= 3) {
            int lc = find_length_code(match.length);
            encode_fixed_lit(static_cast<std::uint16_t>(lc + 257), bw);
            if (len_extra[lc] > 0)
                bw.write(static_cast<std::uint32_t>(match.length - len_base[lc]),
                         len_extra[lc]);

            int dc = find_dist_code(match.distance);
            encode_fixed_dist(static_cast<std::uint8_t>(dc), bw);
            if (dst_extra[dc] > 0)
                bw.write(static_cast<std::uint32_t>(match.distance - dst_base[dc]),
                         dst_extra[dc]);

            // Update hash chains for skipped positions
            auto end = pos + static_cast<std::size_t>(match.length);
            for (std::size_t j = pos + 1; j < end && j + 2 < size; ++j)
                matcher.find(j);
            pos = end;
        } else {
            encode_fixed_lit(data[pos], bw);
            ++pos;
        }
    }

    encode_fixed_lit(256, bw);
    bw.flush();
    return output;
}

} // namespace cfbox::deflate
