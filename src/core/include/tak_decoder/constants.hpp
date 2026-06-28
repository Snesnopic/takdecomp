#ifndef TAK_DECODER_CONSTANTS_HPP
#define TAK_DECODER_CONSTANTS_HPP

#include <array>
#include <cstdint>

namespace takdecomp {
    namespace constants {
        constexpr int FORMAT_DATA_TYPE_BITS = 3;
        constexpr int FORMAT_SAMPLE_RATE_BITS = 18;
        constexpr int FORMAT_BPS_BITS = 5;
        constexpr int FORMAT_CHANNEL_BITS = 4;
        constexpr int FORMAT_VALID_BITS = 5;
        constexpr int FORMAT_CH_LAYOUT_BITS = 6;
        constexpr int SIZE_FRAME_DURATION_BITS = 4;
        constexpr int SIZE_SAMPLES_NUM_BITS = 35;
        constexpr int SUBFRAME_LPC_MODE_BITS = 2;
        constexpr int LAST_FRAME_POS_BITS = 40;
        constexpr int LAST_FRAME_SIZE_BITS = 24;
        constexpr int ENCODER_CODEC_BITS = 6;
        constexpr int ENCODER_PROFILE_BITS = 4;

        constexpr int SAMPLE_RATE_MIN = 6000;
        constexpr int CHANNELS_MIN = 1;
        constexpr int BPS_MIN = 8;

        constexpr int FRAME_HEADER_FLAGS_BITS = 3;
        constexpr uint32_t FRAME_HEADER_SYNC_ID = 0xA0FF;
        constexpr int FRAME_HEADER_SYNC_ID_BITS = 16;
        constexpr int FRAME_HEADER_SAMPLE_COUNT_BITS = 14;
        constexpr int FRAME_HEADER_NO_BITS = 21;
        constexpr int FRAME_DURATION_QUANT_SHIFT = 5;
        constexpr int CRC24_BITS = 24;

        constexpr uint8_t FRAME_FLAG_IS_LAST = 0x1;
        constexpr uint8_t FRAME_FLAG_HAS_INFO = 0x2;
        constexpr uint8_t FRAME_FLAG_HAS_METADATA = 0x4;

        constexpr int MAX_CHANNELS = (1 << FORMAT_CHANNEL_BITS);

        constexpr std::array<int8_t, 4> MC_DMODES = {1, 3, 4, 6};
    } // namespace constants

    enum class CodecType : uint8_t {
        MonoStereo = 2,
        MultiChannel = 4,
    };

    enum class MetaDataType : uint8_t {
        End = 0,
        StreamInfo = 1,
        SeekTable = 2,
        SimpleWaveData = 3,
        Encoder = 4,
        Padding = 5,
        Md5 = 6,
        LastFrame = 7,
    };

    enum class FrameSizeType : uint8_t {
        Fs94ms = 0,
        Fs125ms = 1,
        Fs188ms = 2,
        Fs250ms = 3,
        Fs4096 = 4,
        Fs8192 = 5,
        Fs16384 = 6,
        Fs512 = 7,
        Fs1024 = 8,
        Fs2048 = 9,
    };

    enum class SpeakerFlag : uint32_t {
        None                  = 0x00000,
        Front_Left            = 0x00001,
        Front_Right           = 0x00002,
        Front_Center          = 0x00004,
        Low_Frequency         = 0x00008,
        Back_Left             = 0x00010,
        Back_Right            = 0x00020,
        Front_Left_Of_Center  = 0x00040,
        Front_Right_Of_Center = 0x00080,
        Back_Center           = 0x00100,
        Side_Left             = 0x00200,
        Side_Right            = 0x00400,
        Top_Center            = 0x00800,
        Top_Front_Left        = 0x01000,
        Top_Front_Center      = 0x02000,
        Top_Front_Right       = 0x04000,
        Top_Back_Left         = 0x08000,
        Top_Back_Center       = 0x10000,
        Top_Back_Right        = 0x20000,
        Max_Supported         = 0x20000,
    };
} // namespace takdecomp
#endif // TAK_DECODER_CONSTANTS_HPP
