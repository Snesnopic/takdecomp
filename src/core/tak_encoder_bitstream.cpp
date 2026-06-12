#include "tak_encoder/bitstream_writer.hpp"

namespace takenc {

void BitStreamWriter::init(std::vector<uint8_t>& buffer) {
    // we ignore the passed buffer and use our own
    buffer_.clear();
    bit_buf_ = 0;
    bit_cnt_ = 0;
}

void BitStreamWriter::write_bits(uint32_t val, int bits) {
    if (bits == 0) return;
    
    // TAK writes bits in a specific order (like reading, LSB first or MSB first).
    // Decoder reads bits MSB-first or LSB-first?
    // According to bitstream.cpp, Decoder gets bits from LSB to MSB for some parts, but mostly MSB-first inside the byte, or LSB-first?
    // We will align this with the decoder.
    
    if (bits == 32) {
        val &= 0xFFFFFFFF;
    } else {
        val &= (1U << bits) - 1;
    }
    
    bit_buf_ |= (static_cast<uint64_t>(val) << bit_cnt_);
    bit_cnt_ += bits;

    while (bit_cnt_ >= 8) {
        buffer_.push_back(static_cast<uint8_t>(bit_buf_ & 0xFF));
        bit_buf_ >>= 8;
        bit_cnt_ -= 8;
    }
}

void BitStreamWriter::write_bit(uint32_t val) {
    write_bits(val, 1);
}

void BitStreamWriter::write_bits64(uint64_t val, int n) {
    if (n > 32) {
        write_bits(static_cast<uint32_t>(val), 32);
        write_bits(static_cast<uint32_t>(val >> 32), n - 32);
    } else {
        write_bits(static_cast<uint32_t>(val), n);
    }
}

void BitStreamWriter::align_write_bits() {
    if (bit_cnt_ > 0) {
        buffer_.push_back(static_cast<uint8_t>(bit_buf_ & 0xFF));
        bit_cnt_ = 0;
        bit_buf_ = 0;
    }
}

void BitStreamWriter::flush() {
    align_write_bits();
}

size_t BitStreamWriter::get_pos() const {
    return buffer_.size() * 8 + bit_cnt_;
}

} // namespace takenc
