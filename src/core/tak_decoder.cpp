#include <iostream>
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
#include <iostream>

#include "tak_decoder/tak_crc.hpp"

namespace takdecomp {

namespace {

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

int Decoder::get_nb_samples(int sample_rate, FrameSizeType type) {
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

StreamInfo Decoder::parse_streaminfo(BitStreamReader& gb) {
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
    
    uint32_t crc = compute_crc24(buf, header_len);
    
    if (crc != expected_crc) {
        throw std::runtime_error("CRC mismatch in frame header");
    }
}

size_t Decoder::decode_frame(std::span<const uint8_t> data, StreamInfo& info, std::vector<std::vector<int32_t>>& output) {
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
        gb.align_get_bits();
        uint32_t tail_val = gb.get_bits(24);
        (void)tail_val; // Ignore 24 bits
    }
    
    gb.align_get_bits();
    return gb.get_position_bytes();
}

} // namespace takdecomp

bool takdecomp::Decoder::check_crc24(std::span<const uint8_t> data) {
    if (data.size() < 3) { return false;
}
    
    size_t const len = data.size() - 3;
    uint32_t crc = compute_crc24(data.data(), len);
    
    uint32_t const expected_crc = data[len] | (data[len + 1] << 8) | (data[len + 2] << 16);
    return crc == expected_crc;
}

