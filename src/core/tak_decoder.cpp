#include "tak_decoder/constants.hpp"
#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <cstdint>
#include "tak_decoder/streaminfo.hpp"
#include <cstddef>
#include <span>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace takdecomp {

namespace {

constexpr std::array<uint32_t, 256> crc24_table = {
    0x000000, 0xFB4C86, 0x0DD58A, 0xF6990C, 0xE1E693, 0x1AAA15, 0xEC3319, 0x177F9F,
    0x3981A1, 0xC2CD27, 0x34542B, 0xCF18AD, 0xD86732, 0x232BB4, 0xD5B2B8, 0x2EFE3E,
    0x894EC5, 0x720243, 0x849B4F, 0x7FD7C9, 0x68A856, 0x93E4D0, 0x657DDC, 0x9E315A,
    0xB0CF64, 0x4B83E2, 0xBD1AEE, 0x465668, 0x5129F7, 0xAA6571, 0x5CFC7D, 0xA7B0FB,
    0xE9D10C, 0x129D8A, 0xE40486, 0x1F4800, 0x08379F, 0xF37B19, 0x05E215, 0xFEAE93,
    0xD050AD, 0x2B1C2B, 0xDD8527, 0x26C9A1, 0x31B63E, 0xCAFAB8, 0x3C63B4, 0xC72F32,
    0x609FC9, 0x9BD34F, 0x6D4A43, 0x9606C5, 0x81795A, 0x7A35DC, 0x8CACD0, 0x77E056,
    0x591E68, 0xA252EE, 0x54CBE2, 0xAF8764, 0xB8F8FB, 0x43B47D, 0xB52D71, 0x4E61F7,
    0xD2A319, 0x29EF9F, 0xDF7693, 0x243A15, 0x33458A, 0xC8090C, 0x3E9000, 0xC5DC86,
    0xEB22B8, 0x106E3E, 0xE6F732, 0x1DBBB4, 0x0AC42B, 0xF188AD, 0x0711A1, 0xFC5D27,
    0x5BEDDC, 0xA0A15A, 0x563856, 0xAD74D0, 0xBA0B4F, 0x4147C9, 0xB7DEC5, 0x4C9243,
    0x626C7D, 0x9920FB, 0x6FB9F7, 0x94F571, 0x838AEE, 0x78C668, 0x8E5F64, 0x7513E2,
    0x3B7215, 0xC03E93, 0x36A79F, 0xCDEB19, 0xDA9486, 0x21D800, 0xD7410C, 0x2C0D8A,
    0x02F3B4, 0xF9BF32, 0x0F263E, 0xF46AB8, 0xE31527, 0x1859A1, 0xEEC0AD, 0x158C2B,
    0xB23CD0, 0x497056, 0xBFE95A, 0x44A5DC, 0x53DA43, 0xA896C5, 0x5E0FC9, 0xA5434F,
    0x8BBD71, 0x70F1F7, 0x8668FB, 0x7D247D, 0x6A5BE2, 0x911764, 0x678E68, 0x9CC2EE,
    0xA44733, 0x5F0BB5, 0xA992B9, 0x52DE3F, 0x45A1A0, 0xBEED26, 0x48742A, 0xB338AC,
    0x9DC692, 0x668A14, 0x901318, 0x6B5F9E, 0x7C2001, 0x876C87, 0x71F58B, 0x8AB90D,
    0x2D09F6, 0xD64570, 0x20DC7C, 0xDB90FA, 0xCCEF65, 0x37A3E3, 0xC13AEF, 0x3A7669,
    0x148857, 0xEFC4D1, 0x195DDD, 0xE2115B, 0xF56EC4, 0x0E2242, 0xF8BB4E, 0x03F7C8,
    0x4D963F, 0xB6DAB9, 0x4043B5, 0xBB0F33, 0xAC70AC, 0x573C2A, 0xA1A526, 0x5AE9A0,
    0x74179E, 0x8F5B18, 0x79C214, 0x828E92, 0x95F10D, 0x6EBD8B, 0x982487, 0x636801,
    0xC4D8FA, 0x3F947C, 0xC90D70, 0x3241F6, 0x253E69, 0xDE72EF, 0x28EBE3, 0xD3A765,
    0xFD595B, 0x0615DD, 0xF08CD1, 0x0BC057, 0x1CBFC8, 0xE7F34E, 0x116A42, 0xEA26C4,
    0x76E42A, 0x8DA8AC, 0x7B31A0, 0x807D26, 0x9702B9, 0x6C4E3F, 0x9AD733, 0x619BB5,
    0x4F658B, 0xB4290D, 0x42B001, 0xB9FC87, 0xAE8318, 0x55CF9E, 0xA35692, 0x581A14,
    0xFFAAEF, 0x04E669, 0xF27F65, 0x0933E3, 0x1E4C7C, 0xE500FA, 0x1399F6, 0xE8D570,
    0xC62B4E, 0x3D67C8, 0xCBFEC4, 0x30B242, 0x27CDDD, 0xDC815B, 0x2A1857, 0xD154D1,
    0x9F3526, 0x6479A0, 0x92E0AC, 0x69AC2A, 0x7ED3B5, 0x859F33, 0x73063F, 0x884AB9,
    0xA6B487, 0x5DF801, 0xAB610D, 0x502D8B, 0x475214, 0xBC1E92, 0x4A879E, 0xB1CB18,
    0x167BE3, 0xED3765, 0x1BAE69, 0xE0E2EF, 0xF79D70, 0x0CD1F6, 0xFA48FA, 0x01047C,
    0x2FFA42, 0xD4B6C4, 0x222FC8, 0xD9634E, 0xCE1CD1, 0x355057, 0xC3C95B, 0x3885DD,
};

constexpr std::array<int64_t, 19> tak_channel_layouts = {
    0,
    0x00000001, // AV_CH_FRONT_LEFT
    0x00000002, // AV_CH_FRONT_RIGHT
    0x00000004, // AV_CH_FRONT_CENTER
    0x00000008, // AV_CH_LOW_FREQUENCY
    0x00000010, // AV_CH_BACK_LEFT
    0x00000020, // AV_CH_BACK_RIGHT
    0x00000040, // AV_CH_FRONT_LEFT_OF_CENTER
    0x00000080, // AV_CH_FRONT_RIGHT_OF_CENTER
    0x00000100, // AV_CH_BACK_CENTER
    0x00000200, // AV_CH_SIDE_LEFT
    0x00000400, // AV_CH_SIDE_RIGHT
    0x00000800, // AV_CH_TOP_CENTER
    0x00001000, // AV_CH_TOP_FRONT_LEFT
    0x00002000, // AV_CH_TOP_FRONT_CENTER
    0x00004000, // AV_CH_TOP_FRONT_RIGHT
    0x00008000, // AV_CH_TOP_BACK_LEFT
    0x00010000, // AV_CH_TOP_BACK_CENTER
    0x00020000, // AV_CH_TOP_BACK_RIGHT
};

constexpr std::array<uint16_t, 10> frame_duration_type_quants = {
    3, 4, 6, 8, 4096, 8192, 16384, 512, 1024, 2048,
};

} // namespace

auto Decoder::get_nb_samples(int sample_rate, FrameSizeType type) -> int {
    int nb_samples = 0;
    int max_nb_samples = 0;
    auto const type_val = static_cast<uint8_t>(type);

    if (type_val <= static_cast<uint8_t>(FrameSizeType::Fs250ms)) {
        nb_samples = (sample_rate * frame_duration_type_quants[type_val]) >> constants::FRAME_DURATION_QUANT_SHIFT;
        max_nb_samples = 16384;
    } else if (type_val < frame_duration_type_quants.size()) {
        nb_samples = frame_duration_type_quants[type_val];
        max_nb_samples = (sample_rate * frame_duration_type_quants[static_cast<uint8_t>(FrameSizeType::Fs250ms)]) >> constants::FRAME_DURATION_QUANT_SHIFT;
    } else {
        throw std::runtime_error("Invalid frame size type");
    }

    if (nb_samples <= 0 || nb_samples > max_nb_samples) {
        throw std::runtime_error("Invalid number of samples");
    }

    return nb_samples;
}

auto Decoder::parse_streaminfo(BitStreamReader& gb) -> StreamInfo {
    StreamInfo s;

    s.codec = static_cast<CodecType>(gb.get_bits(constants::ENCODER_CODEC_BITS));
    gb.skip_bits(constants::ENCODER_PROFILE_BITS);

    auto frame_type = static_cast<FrameSizeType>(gb.get_bits(constants::SIZE_FRAME_DURATION_BITS));
    s.samples = gb.get_bits64(constants::SIZE_SAMPLES_NUM_BITS);

    s.data_type = gb.get_bits(constants::FORMAT_DATA_TYPE_BITS);
    s.sample_rate = gb.get_bits(constants::FORMAT_SAMPLE_RATE_BITS) + constants::SAMPLE_RATE_MIN;
    s.bps = gb.get_bits(constants::FORMAT_BPS_BITS) + constants::BPS_MIN;
    s.channels = gb.get_bits(constants::FORMAT_CHANNEL_BITS) + constants::CHANNELS_MIN;

    uint64_t channel_mask = 0;
    if (gb.get_bits1() != 0u) {
        gb.skip_bits(constants::FORMAT_VALID_BITS);
        if (gb.get_bits1() != 0u) {
            for (int i = 0; i < s.channels; i++) {
                int const value = gb.get_bits(constants::FORMAT_CH_LAYOUT_BITS);
                if (std::cmp_less(value ,tak_channel_layouts.size())) {
                    channel_mask |= tak_channel_layouts[value];
                }
            }
        }
    }

    s.ch_layout = channel_mask;
    s.frame_samples = get_nb_samples(s.sample_rate, frame_type);

    return s;
}

void Decoder::decode_frame_header(BitStreamReader& gb, StreamInfo& ti) {
    if (gb.get_bits(constants::FRAME_HEADER_SYNC_ID_BITS) != constants::FRAME_HEADER_SYNC_ID) {
        throw std::runtime_error("Missing sync id");
    }

    ti.flags = gb.get_bits(constants::FRAME_HEADER_FLAGS_BITS);
    ti.frame_num = gb.get_bits(constants::FRAME_HEADER_NO_BITS);

    if ((ti.flags & constants::FRAME_FLAG_IS_LAST) != 0) {
        ti.last_frame_samples = gb.get_bits(constants::FRAME_HEADER_SAMPLE_COUNT_BITS) + 1;
        gb.skip_bits(2);
    } else {
        ti.last_frame_samples = 0;
    }

    if ((ti.flags & constants::FRAME_FLAG_HAS_INFO) != 0) {
        parse_streaminfo(gb);
        
        if (gb.get_bits(6) != 0u) {
            gb.skip_bits(25);
        }
        gb.align_get_bits();
    }

    if ((ti.flags & constants::FRAME_FLAG_HAS_METADATA) != 0) {
        throw std::runtime_error("Metadata decoding not supported yet");
    }

    if (gb.get_bits_left() < 24) {
        throw std::runtime_error("Not enough bits for CRC");
    }

    size_t const header_len = gb.get_position_bytes();
    
    // Read the expected CRC as a 24-bit big-endian integer from the aligned byte position
    const uint8_t* buf = gb.get_data().data();
    uint32_t const expected_crc = (buf[header_len] << 16) | (buf[header_len + 1] << 8) | buf[header_len + 2];
    
    gb.skip_bits(24);
    
    uint32_t crc = 0xCE04B7;
    for (size_t i = 0; i < header_len; i++) {
        crc = crc24_table[static_cast<uint8_t>(crc) ^ buf[i]] ^ (crc >> 8);
    }
    crc &= 0xFFFFFF;
    
    if (crc != expected_crc) {
        throw std::runtime_error("CRC mismatch in frame header");
    }
}

auto Decoder::decode_frame(std::span<const uint8_t> data, StreamInfo& info, std::vector<std::vector<int32_t>>& output) -> size_t {
    BitStreamReader gb(data);
    decode_frame_header(gb, info);

    bps_ = info.bps;
    channels_ = info.channels;
    sample_rate_ = info.sample_rate;
    
    if (info.codec != CodecType::MonoStereo && info.codec != CodecType::MultiChannel) {
        throw std::runtime_error("Unsupported codec type");
    }
    
    int shift = 0;
    if (sample_rate_ < 11025) {
        shift = 3;
    } else if (sample_rate_ < 22050) {
        shift = 2;
    } else if (sample_rate_ < 44100) {
        shift = 1;
    } else {
        shift = 0;
    }
    
    int64_t const base_align = (((sample_rate_ + 511LL) >> 9) + 3) & ~3LL;
    uval_ = base_align << shift;
    subframe_scale_ = base_align << 1;

    nb_samples_ = (info.last_frame_samples != 0) ? info.last_frame_samples : info.frame_samples;
    
    decoded_.resize(channels_);
    for (int i = 0; i < channels_; ++i) {
        decoded_[i].resize(nb_samples_);
    }

    if (nb_samples_ < 16) {
        for (int chan = 0; chan < channels_; chan++) {
            for (int i = 0; i < nb_samples_; i++) {
                decoded_[chan][i] = gb.get_sbits(bps_);
            }
        }
    } else {
        if (info.codec == CodecType::MonoStereo) {
            for (int chan = 0; chan < channels_; chan++) {
                decode_channel(chan, gb);
            }

            if (channels_ == 2) {
                nb_subframes_ = gb.get_bits1() + 1;

                if (nb_subframes_ > 1) {
                    subframe_len_[1] = gb.get_bits(6);
                }

                dmode_ = gb.get_bits(3);
                if (dmode_ == 7) { throw std::runtime_error("Invalid dmode");
}
                decorrelate(0, 1, nb_samples_ - 1, gb);
            }
        } else if (info.codec == CodecType::MultiChannel) {
            if (gb.get_bits1() != 0u) {
                int ch_mask = 0;
                int const chan = gb.get_bits(4) + 1;
                if (chan > channels_) { throw std::runtime_error("Invalid multichannel chan");
}

                for (int i = 0; i < chan; i++) {
                    int const nbit = gb.get_bits(4);
                    if (nbit >= channels_ || ((ch_mask & (1 << nbit)) != 0)) { throw std::runtime_error("Invalid channel mask");
}

                    mcdparams_[i].present = gb.get_bits1();
                    if (mcdparams_[i].present != 0) {
                        mcdparams_[i].index = gb.get_bits(2);
                        mcdparams_[i].chan2 = gb.get_bits(4);
                        if (mcdparams_[i].chan2 >= channels_) { throw std::runtime_error("Invalid chan2");
}
                        if (mcdparams_[i].index == 1) {
                            if (nbit == mcdparams_[i].chan2 || ((ch_mask & (1 << mcdparams_[i].chan2)) != 0)) {
                                throw std::runtime_error("Invalid multichannel config");
                            }
                            ch_mask |= 1 << mcdparams_[i].chan2;
                        } else if ((ch_mask & (1 << mcdparams_[i].chan2)) == 0) {
                            throw std::runtime_error("Invalid multichannel config");
                        }
                    }
                    mcdparams_[i].chan1 = nbit;
                    ch_mask |= 1 << nbit;
                }
            } else {
                for (int i = 0; i < channels_; i++) {
                    mcdparams_[i].present = 0;
                    mcdparams_[i].chan1 = i;
                }
            }

            for (int i = 0; i < channels_; i++) {
                if ((mcdparams_[i].present != 0) && mcdparams_[i].index == 1) {
                    decode_channel(mcdparams_[i].chan2, gb);
                }

                decode_channel(mcdparams_[i].chan1, gb);

                if (mcdparams_[i].present != 0) {
                    dmode_ = constants::MC_DMODES[mcdparams_[i].index];
                    decorrelate(mcdparams_[i].chan2, mcdparams_[i].chan1, nb_samples_ - 1, gb);
                }
            }
        }

        for (int chan = 0; chan < channels_; chan++) {
            sample_shift_[chan] = (gb.get_bits1() != 0u) ? gb.get_bits(4) + 1 : 0;
            if (lpc_mode_[chan] != 0) {
                decode_lpc(decoded_[chan].data(), lpc_mode_[chan], nb_samples_);
            }

            if (sample_shift_[chan] > 0) {
                for (int i = 0; i < nb_samples_; i++) {
                    decoded_[chan][i] *= 1 << sample_shift_[chan];
                }
            }
        }
    }
    
    // Copy out
    output = decoded_;
    
    if (gb.get_bits_left() >= 24) {
        gb.skip_bits(24);
    }
    
    gb.align_get_bits();
    return gb.get_position_bytes();
}

} // namespace takdecomp

auto takdecomp::Decoder::check_crc24(std::span<const uint8_t> data) -> bool {
    if (data.size() < 3) { return false;
}
    
    uint32_t crc = 0xCE04B7;
    size_t const len = data.size() - 3;
    
    for (size_t i = 0; i < len; i++) {
        crc = crc24_table[static_cast<uint8_t>(crc) ^ data[i]] ^ (crc >> 8);
    }
    crc &= 0xFFFFFF;
    
    uint32_t const expected_crc = data[len] | (data[len + 1] << 8) | (data[len + 2] << 16);
    return crc == expected_crc;
}

