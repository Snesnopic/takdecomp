#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <bit>

namespace takdecomp {

class BitStreamReader {
public:
    explicit BitStreamReader(std::span<const uint8_t> data) 
        : data_(data), byte_idx_(0), bit_idx_(0) {}

    uint32_t get_bits(int n) {
        if (n <= 0) return 0;
        if (n > 32) throw std::invalid_argument("get_bits supports up to 32 bits");

        uint32_t result = 0;
        for (int i = 0; i < n; ++i) {
            result |= (static_cast<uint32_t>(get_bits1()) << i);
        }
        return result;
    }

    uint64_t get_bits64(int n) {
        if (n <= 0) return 0;
        if (n > 64) throw std::invalid_argument("get_bits64 supports up to 64 bits");

        uint64_t result = 0;
        for (int i = 0; i < n; ++i) {
            result |= (static_cast<uint64_t>(get_bits1()) << i);
        }
        return result;
    }

    int32_t get_sbits(int n) {
        if (n <= 0) return 0;
        uint32_t val = get_bits(n);
        int32_t sval = val;
        if (val & (1 << (n - 1))) {
            sval -= (1 << n);
        }
        return sval;
    }

    uint8_t get_bits1() {
        if (byte_idx_ >= data_.size()) {
            return 0;
        }
        // TAK uses Little Endian bitstream (LSB first)
        uint8_t bit = (data_[byte_idx_] >> bit_idx_) & 1;
        
        bit_idx_++;
        if (bit_idx_ == 8) {
            bit_idx_ = 0;
            byte_idx_++;
        }
        return bit;
    }

    void skip_bits(int n) {
        if (n <= 0) return;
        if (get_bits_left() < n) throw std::out_of_range("Not enough bits left to skip");
        
        int total_bits = bit_idx_ + n;
        byte_idx_ += total_bits / 8;
        bit_idx_ = total_bits % 8;
    }

    void align_get_bits() {
        if (bit_idx_ != 0) {
            bit_idx_ = 0;
            byte_idx_++;
        }
    }

    size_t get_bits_left() const {
        if (byte_idx_ >= data_.size()) return 0;
        return (data_.size() - byte_idx_) * 8 - bit_idx_;
    }

    std::span<const uint8_t> get_data() const { return data_; }

    size_t get_position_bytes() const { return byte_idx_; }
    int get_bit_idx() const { return bit_idx_; }

private:
    std::span<const uint8_t> data_;
    size_t byte_idx_;
    int bit_idx_;
};

} // namespace takdecomp
