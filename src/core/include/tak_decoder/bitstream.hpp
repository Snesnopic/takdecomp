#ifndef TAK_DECODER_BITSTREAM_HPP
#define TAK_DECODER_BITSTREAM_HPP

#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace takdecomp {
    class BitStreamReader {
    public:
        explicit BitStreamReader(const std::span<const uint8_t> data)
            : data_(data) {
        }

        uint32_t get_bits(const int n) {
            if (n <= 0) {
                return 0;
            }
            if (n > 32) {
                throw std::invalid_argument("get_bits supports up to 32 bits");
            }

            uint32_t result = 0;
            for (int i = 0; i < n; ++i) {
                result |= (static_cast<uint32_t>(get_bits1()) << i);
            }
            return result;
        }

        uint64_t get_bits64(const int n) {
            if (n <= 0) {
                return 0;
            }
            if (n > 64) {
                throw std::invalid_argument("get_bits64 supports up to 64 bits");
            }

            uint64_t result = 0;
            for (int i = 0; i < n; ++i) {
                result |= (static_cast<uint64_t>(get_bits1()) << i);
            }
            return result;
        }

        int32_t get_sbits(const int n) {
            if (n <= 0) {
                return 0;
            }
            uint32_t const val = get_bits(n);
            int32_t sval = val;
            if ((val & (1 << (n - 1))) != 0u) {
                sval -= (1 << n);
            }
            return sval;
        }

        uint8_t get_bits1() {
            if (byte_idx_ >= data_.size()) {
                return 0;
            }
            // TAK uses Little Endian bitstream (LSB first)
            uint8_t const bit = (data_[byte_idx_] >> bit_idx_) & 1;

            bit_idx_++;
            if (bit_idx_ == 8) {
                bit_idx_ = 0;
                byte_idx_++;
            }
            return bit;
        }

        void skip_bits(const int n) {
            if (n <= 0) {
                return;
            }
            if (get_bits_left() < static_cast<size_t>(n)) {
                throw std::out_of_range("Not enough bits left to skip");
            }

            int const total_bits = bit_idx_ + n;
            byte_idx_ += total_bits / 8;
            bit_idx_ = total_bits % 8;
        }

        void align_get_bits() {
            if (bit_idx_ != 0) {
                bit_idx_ = 0;
                byte_idx_++;
            }
        }

        [[nodiscard]] size_t get_bits_left() const {
            if (byte_idx_ >= data_.size()) {
                return 0;
            }
            return ((data_.size() - byte_idx_) * 8) - bit_idx_;
        }

        [[nodiscard]] std::span<const uint8_t> get_data() const { return data_; }

        [[nodiscard]] size_t get_position_bytes() const { return byte_idx_; }
        [[nodiscard]] int get_bit_idx() const { return bit_idx_; }

    private:
        std::span<const uint8_t> data_;
        size_t byte_idx_{0};
        int bit_idx_{0};
    };
} // namespace takdecomp
#endif // TAK_DECODER_BITSTREAM_HPP
