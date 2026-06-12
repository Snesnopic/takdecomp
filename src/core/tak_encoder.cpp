#include "tak_encoder/encoder.hpp"
#include "tak_encoder/decorrelate.hpp"
#include "tak_decoder/constants.hpp"
#include "tak_decoder/tak_crc.hpp"
#include "tak_encoder/bitstream_writer.hpp"
#include "tak_encoder/wav_reader.hpp"
#include "tak_encoder/filter.hpp"
#include "tak_encoder/subframe.hpp"
#include "tak_encoder/md5.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cmath>
#include <array>
#include <algorithm>

namespace takenc {

EncodeResult Encoder::encode_file(const char* wav_path, const char* tak_path, ProgressCallback progress) {
    std::ifstream is(wav_path, std::ios::binary);
    if (!is) throw std::runtime_error("Could not open WAV file");
    std::ofstream os(tak_path, std::ios::binary);
    if (!os) throw std::runtime_error("Could not open TAK file for writing");
    return encode_stream(is, os, progress);
}

EncodeResult Encoder::encode_stream(std::istream& is, std::ostream& os, ProgressCallback progress) {
    WavInfo wav = read_wav_header(is);
    int channels = wav.channels;
    int bps = wav.bps;
    int sample_rate = wav.sample_rate;
    int block_align = channels * (bps / 8);
    int total_samples = wav.data_size / block_align;

    os.write("tBaK", 4);

    // StreamInfo metadata
    BitStreamWriter si_gb;
    si_gb.write_bits(static_cast<int>(takdecomp::CodecType::MonoStereo),
                     takdecomp::constants::ENCODER_CODEC_BITS);
    si_gb.write_bits(0, takdecomp::constants::ENCODER_PROFILE_BITS);
    si_gb.write_bits(static_cast<int>(takdecomp::FrameSizeType::Fs4096),
                     takdecomp::constants::SIZE_FRAME_DURATION_BITS);
    si_gb.write_bits64(total_samples, takdecomp::constants::SIZE_SAMPLES_NUM_BITS);
    si_gb.write_bits(0, takdecomp::constants::FORMAT_DATA_TYPE_BITS);
    si_gb.write_bits(sample_rate - takdecomp::constants::SAMPLE_RATE_MIN,
                     takdecomp::constants::FORMAT_SAMPLE_RATE_BITS);
    si_gb.write_bits(bps - takdecomp::constants::BPS_MIN,
                     takdecomp::constants::FORMAT_BPS_BITS);
    si_gb.write_bits(channels - takdecomp::constants::CHANNELS_MIN,
                     takdecomp::constants::FORMAT_CHANNEL_BITS);
    si_gb.write_bit(0);
    si_gb.align_write_bits();

    os.write("\x01", 1);
    int si_len = si_gb.get_position_bytes();
    int md_block_size = si_len + 3;
    uint8_t size_le[3] = {
        static_cast<uint8_t>(md_block_size & 0xff),
        static_cast<uint8_t>((md_block_size >> 8) & 0xff),
        static_cast<uint8_t>((md_block_size >> 16) & 0xff)
    };
    os.write(reinterpret_cast<char*>(size_le), 3);
    os.write(reinterpret_cast<const char*>(si_gb.get_data().data()), si_len);
    uint32_t si_crc = takdecomp::compute_crc24(si_gb.get_data().data(), si_len);
    uint8_t crc_le[3] = {
        static_cast<uint8_t>(si_crc & 0xff),
        static_cast<uint8_t>((si_crc >> 8) & 0xff),
        static_cast<uint8_t>((si_crc >> 16) & 0xff)
    };
    os.write(reinterpret_cast<char*>(crc_le), 3);
    os.write("\x00\x00\x00\x00", 4); // End of metadata

    // Encode frames
    int frame_samples = 4096;
    int remaining_samples = total_samples;
    int frame_num = 0;
    Decorrelator decorr;
    MD5 md5;

    while (remaining_samples > 0) {
        if (progress) {
            progress(total_samples - remaining_samples, total_samples);
        }

        int current_frame_samples = std::min(frame_samples, remaining_samples);
        std::vector<uint8_t> raw_bytes(current_frame_samples * block_align);
        is.read(reinterpret_cast<char*>(raw_bytes.data()), raw_bytes.size());
        md5.update(raw_bytes.data(), raw_bytes.size());

        std::vector<int32_t> c1(current_frame_samples);
        std::vector<int32_t> c2(current_frame_samples);

        int byte_idx = 0;
        for (int i = 0; i < current_frame_samples; i++) {
            if (bps == 16) {
                if (channels == 2) {
                    int16_t l = raw_bytes[byte_idx] | (raw_bytes[byte_idx+1] << 8);
                    int16_t r = raw_bytes[byte_idx+2] | (raw_bytes[byte_idx+3] << 8);
                    c1[i] = l; c2[i] = r;
                    byte_idx += 4;
                } else {
                    int16_t l = raw_bytes[byte_idx] | (raw_bytes[byte_idx+1] << 8);
                    c1[i] = l;
                    byte_idx += 2;
                }
            } else if (bps == 24) {
                if (channels == 2) {
                    int32_t l = raw_bytes[byte_idx] | (raw_bytes[byte_idx+1] << 8) | (raw_bytes[byte_idx+2] << 16);
                    if (l & 0x800000) l |= 0xFF000000;
                    int32_t r = raw_bytes[byte_idx+3] | (raw_bytes[byte_idx+4] << 8) | (raw_bytes[byte_idx+5] << 16);
                    if (r & 0x800000) r |= 0xFF000000;
                    c1[i] = l; c2[i] = r;
                    byte_idx += 6;
                } else {
                    int32_t l = raw_bytes[byte_idx] | (raw_bytes[byte_idx+1] << 8) | (raw_bytes[byte_idx+2] << 16);
                    if (l & 0x800000) l |= 0xFF000000;
                    c1[i] = l;
                    byte_idx += 3;
                }
            }
        }

        // Frame header
        BitStreamWriter fw;
        fw.write_bits(takdecomp::constants::FRAME_HEADER_SYNC_ID,
                      takdecomp::constants::FRAME_HEADER_SYNC_ID_BITS);
        int flags = 0;
        if (remaining_samples <= frame_samples)
            flags |= takdecomp::constants::FRAME_FLAG_IS_LAST;
        fw.write_bits(flags, takdecomp::constants::FRAME_HEADER_FLAGS_BITS);
        fw.write_bits(frame_num, takdecomp::constants::FRAME_HEADER_NO_BITS);
        if (flags & takdecomp::constants::FRAME_FLAG_IS_LAST) {
            fw.write_bits(current_frame_samples - 1,
                          takdecomp::constants::FRAME_HEADER_SAMPLE_COUNT_BITS);
            fw.write_bits(0, 2);
        }
        fw.align_write_bits();
        int header_bytes = fw.get_position_bytes();
        uint32_t header_crc = takdecomp::compute_crc24(fw.get_data().data(), header_bytes);
        fw.write_bits((header_crc >> 16) & 0xff, 8);
        fw.write_bits((header_crc >> 8) & 0xff, 8);
        fw.write_bits(header_crc & 0xff, 8);

        // Choose LPC mode
        int lpc_mode_c1 = 0, lpc_mode_c2 = 0;
        if (current_frame_samples >= 16) {
            int costs[4];
            for (int m = 0; m < 4; m++) costs[m] = estimate_lpc_cost(c1.data(), current_frame_samples, m);
            lpc_mode_c1 = 0;
            for (int m = 1; m < 4; m++) if (costs[m] < costs[lpc_mode_c1]) lpc_mode_c1 = m;

            if (channels == 2) {
                for (int m = 0; m < 4; m++) costs[m] = estimate_lpc_cost(c2.data(), current_frame_samples, m);
                lpc_mode_c2 = 0;
                for (int m = 1; m < 4; m++) if (costs[m] < costs[lpc_mode_c2]) lpc_mode_c2 = m;
            }
        }

        // Apply inverse LPC
        inverse_lpc(c1.data(), lpc_mode_c1, current_frame_samples);
        if (channels == 2) inverse_lpc(c2.data(), lpc_mode_c2, current_frame_samples);

        // Apply inverse decorrelation
        int best_dmode = 0;
        int32_t pre_c1 = c1[0], pre_c2 = (channels == 2) ? c2[0] : 0;
        if (channels == 2) {
            best_dmode = decorr.apply_decorrelation(c1.data(), c2.data(), current_frame_samples);
            c1[0] = pre_c1; c2[0] = pre_c2;
        }

        // Encode channels
        for (int ch = 0; ch < channels; ch++) {
            const int32_t* d = (ch == 0) ? c1.data() : c2.data();
            int lpc = (ch == 0) ? lpc_mode_c1 : lpc_mode_c2;
            encode_channel(d, current_frame_samples, bps, lpc, sample_rate, fw);
        }

        // Stereo decorrelation info
        if (channels == 2) {
            fw.write_bit(0);
            fw.write_bits(best_dmode, 3);
        }

        // Final sample_shift
        for (int ch = 0; ch < channels; ch++) fw.write_bit(0);

        // Frame tail
        fw.align_write_bits();
        fw.write_bits(0, 24);

        os.write(reinterpret_cast<const char*>(fw.get_data().data()),
                 fw.get_position_bytes());
        remaining_samples -= current_frame_samples;
        frame_num++;
    }

    if (progress) {
        progress(total_samples, total_samples);
    }
    
    md5.finalize();
    return EncodeResult{ md5.to_string() };
}

} // namespace takenc
