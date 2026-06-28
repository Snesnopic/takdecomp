#ifndef TAK_ENCODER_WAV_READER_HPP
#define TAK_ENCODER_WAV_READER_HPP

#include <iostream>
#include <cstdint>
#include <vector>

namespace takenc {
    struct WavInfo {
        uint16_t channels;
        uint32_t sample_rate;
        uint16_t bps;
        uint32_t data_size;
        std::vector<uint8_t> fmt_chunk;

        struct ForeignChunk {
            char id[4];
            std::vector<uint8_t> data;
        };

        std::vector<ForeignChunk> foreign_chunks;
        std::vector<uint8_t> header_data;
    };

    WavInfo read_wav_header(std::istream &is);
} // namespace takenc

#endif // TAK_ENCODER_WAV_READER_HPP
