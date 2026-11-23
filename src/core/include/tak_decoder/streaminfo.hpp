// SPDX-License-Identifier: LGPL-2.1-or-later
// derived from FFmpeg libavcodec/takdec.c, tak.c, tak.h and takdsp.c, Copyright (c) 2012, 2015 Paul B Mahol
#ifndef TAK_DECODER_STREAMINFO_HPP
#define TAK_DECODER_STREAMINFO_HPP

#include "constants.hpp"
#include <cstdint>

namespace takdecomp {
    /**
     * @brief Structure containing the metadata of a TAK bitstream.
     * 
     * This object holds global stream properties such as sample rate, 
     * bit depth, channel count, and the total number of audio samples.
     */
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
        int preset = 0;
    };
} // namespace takdecomp
#endif // TAK_DECODER_STREAMINFO_HPP
