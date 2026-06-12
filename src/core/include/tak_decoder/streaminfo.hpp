#ifndef TAK_DECODER_STREAMINFO_HPP
#define TAK_DECODER_STREAMINFO_HPP

#include "constants.hpp"
#include <cstdint>

namespace takdecomp {

struct StreamInfo {
    int flags = 0;
    CodecType codec = CodecType::MonoStereo;
    int data_type = 0;
    int sample_rate = 0;
    int channels = 0;
    int bps = 0;
    int frame_num = 0;
    int frame_samples = 0;
    int last_frame_samples = 0;
    uint64_t ch_layout = 0;
    int64_t samples = 0;
} __attribute__((packed)) __attribute__((aligned(64)));

} // namespace takdecomp
#endif // TAK_DECODER_STREAMINFO_HPP
