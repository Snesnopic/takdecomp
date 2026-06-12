#ifndef TAK_ENCODER_BITSTREAM_WRITER_HPP
#define TAK_ENCODER_BITSTREAM_WRITER_HPP

#include <cstdint>
#include <vector>
#include <span>
#include <stdexcept>

namespace takenc {

class BitStreamWriter {
public:
    BitStreamWriter() = default;

    void init(std::vector<uint8_t>& buffer);

    void write_bits(uint32_t val, int n);
    void write_bits64(uint64_t val, int n);
    void align_write_bits();

    std::vector<uint8_t>& get_data() { return buffer_; }
    const std::vector<uint8_t>& get_data() const { return buffer_; }
    int get_position_bytes() const { return static_cast<int>(buffer_.size()); }
    void write_bit(uint32_t val);
    void flush();

    [[nodiscard]] size_t get_pos() const;

private:
    std::vector<uint8_t> buffer_;
    uint64_t bit_buf_{0};
    int bit_cnt_{0};
};

} // namespace takenc

#endif // TAK_ENCODER_BITSTREAM_WRITER_HPP
