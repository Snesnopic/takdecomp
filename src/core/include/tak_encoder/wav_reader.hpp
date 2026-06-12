#ifndef TAK_ENCODER_WAV_READER_HPP
#define TAK_ENCODER_WAV_READER_HPP

#include <iostream>
#include <cstdint>

namespace takenc {

struct WavInfo {
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bps;
    uint32_t data_size;
};

WavInfo read_wav_header(std::istream& is);

} // namespace takenc

#endif // TAK_ENCODER_WAV_READER_HPP
