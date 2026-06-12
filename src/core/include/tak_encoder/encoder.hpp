#ifndef TAK_ENCODER_ENCODER_HPP
#define TAK_ENCODER_ENCODER_HPP

#include <cstdint>
#include <vector>
#include <span>
#include "tak_encoder/bitstream_writer.hpp"

namespace takenc {

struct CParam {
    int8_t init;
    uint32_t escape;
    uint32_t scale;
    uint32_t aescape;
    uint32_t bias;
};

extern const CParam xcodes[50];

class Encoder {
public:
    Encoder() = default;

    static void encode_segment(int mode, const int32_t* data, int len, BitStreamWriter& bw);
    static int calc_bits_needed(int mode, const int32_t* data, int len);

    static void encode_file(const char* wav_path, const char* tak_path);
};

} // namespace takenc

#endif // TAK_ENCODER_ENCODER_HPP
