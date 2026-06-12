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

// Helper: write a metadata block (type byte, 3-byte LE size, payload, 3-byte BE CRC)
static void write_metadata_block(std::ostream& os, uint8_t type_byte,
                                  const uint8_t* payload, int payload_len) {
    os.write(reinterpret_cast<const char*>(&type_byte), 1);
    int block_size = payload_len + 3; // payload + CRC
    uint8_t size_le[3] = {
        static_cast<uint8_t>(block_size & 0xff),
        static_cast<uint8_t>((block_size >> 8) & 0xff),
        static_cast<uint8_t>((block_size >> 16) & 0xff)
    };
    os.write(reinterpret_cast<char*>(size_le), 3);
    os.write(reinterpret_cast<const char*>(payload), payload_len);
    uint32_t crc = takdecomp::compute_crc24(payload, payload_len);
    uint8_t crc_be[3] = {
        static_cast<uint8_t>((crc >> 16) & 0xff),
        static_cast<uint8_t>((crc >> 8) & 0xff),
        static_cast<uint8_t>(crc & 0xff)
    };
    os.write(reinterpret_cast<char*>(crc_be), 3);
}

EncodeResult Encoder::encode_file(const char* wav_path, const char* tak_path, const EncoderConfig& cfg, ProgressCallback progress) {
    std::ifstream is(wav_path, std::ios::binary);
    if (!is) throw std::runtime_error("Could not open WAV file");
    std::ofstream os(tak_path, std::ios::binary);
    if (!os) throw std::runtime_error("Could not open TAK file for writing");
    return encode_stream(is, os, cfg, progress);
}

EncodeResult Encoder::encode_stream(std::istream& is, std::ostream& os, const EncoderConfig& cfg, ProgressCallback progress) {
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
    // Use Fs250ms for all presets
    si_gb.write_bits(static_cast<int>(takdecomp::FrameSizeType::Fs250ms),
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

    // --- StreamInfo metadata block (Type 1) ---
    int si_len = si_gb.get_position_bytes();
    write_metadata_block(os, 0x01, si_gb.get_data().data(), si_len);

    // --- LastFrame metadata block (Type 7, placeholder) ---
    // NOTE: is_last bit must NOT be set (0x07, not 0x87)
    os.write("\x07", 1);
    uint8_t lf_size_le[3] = { 11, 0, 0 }; // 8 payload + 3 CRC = 11
    os.write(reinterpret_cast<char*>(lf_size_le), 3);
    
    size_t last_frame_md_offset = os.tellp();
    // Write placeholder payload (will be patched at the end)
    uint8_t lf_placeholder[8] = {0};
    os.write(reinterpret_cast<const char*>(lf_placeholder), 8);
    os.write("\x00\x00\x00", 3); // Dummy CRC (will be patched)

    // --- EncoderInfo metadata block (Type 4) ---
    // Payload: version/profile info matching takc.exe
    uint8_t enc_info[4] = { 0x03, 0x03, 0x02, 0x00 };
    write_metadata_block(os, 0x04, enc_info, 4);

    // --- SimpleWaveData metadata block (Type 3) ---
    // Stores the original WAV header for perfect reconstruction
    {
        // Reconstruct standard 44-byte PCM WAV header
        uint32_t wav_header_size = 44;
        uint32_t data_chunk_size = total_samples * channels * (bps / 8);
        uint32_t riff_size = 36 + data_chunk_size;
        uint32_t byte_rate = sample_rate * channels * (bps / 8);
        uint16_t block_align_wav = channels * (bps / 8);

        // SimpleWaveData format: uint32_t header_size (LE) + uint16_t flags + raw WAV header
        std::vector<uint8_t> swd_payload(6 + wav_header_size);
        // First 4 bytes: WAV header size
        swd_payload[0] = wav_header_size & 0xFF;
        swd_payload[1] = (wav_header_size >> 8) & 0xFF;
        swd_payload[2] = (wav_header_size >> 16) & 0xFF;
        swd_payload[3] = (wav_header_size >> 24) & 0xFF;
        // Next 2 bytes: flags (0)
        swd_payload[4] = 0;
        swd_payload[5] = 0;
        // WAV header (44 bytes)
        uint8_t* w = swd_payload.data() + 6;
        std::memcpy(w, "RIFF", 4); w += 4;
        w[0] = riff_size & 0xFF; w[1] = (riff_size >> 8) & 0xFF;
        w[2] = (riff_size >> 16) & 0xFF; w[3] = (riff_size >> 24) & 0xFF; w += 4;
        std::memcpy(w, "WAVE", 4); w += 4;
        std::memcpy(w, "fmt ", 4); w += 4;
        uint32_t fmt_size = 16;
        w[0] = fmt_size & 0xFF; w[1] = (fmt_size >> 8) & 0xFF;
        w[2] = (fmt_size >> 16) & 0xFF; w[3] = (fmt_size >> 24) & 0xFF; w += 4;
        uint16_t audio_format = 1; // PCM
        w[0] = audio_format & 0xFF; w[1] = (audio_format >> 8) & 0xFF; w += 2;
        w[0] = channels & 0xFF; w[1] = (channels >> 8) & 0xFF; w += 2;
        w[0] = sample_rate & 0xFF; w[1] = (sample_rate >> 8) & 0xFF;
        w[2] = (sample_rate >> 16) & 0xFF; w[3] = (sample_rate >> 24) & 0xFF; w += 4;
        w[0] = byte_rate & 0xFF; w[1] = (byte_rate >> 8) & 0xFF;
        w[2] = (byte_rate >> 16) & 0xFF; w[3] = (byte_rate >> 24) & 0xFF; w += 4;
        w[0] = block_align_wav & 0xFF; w[1] = (block_align_wav >> 8) & 0xFF; w += 2;
        w[0] = bps & 0xFF; w[1] = (bps >> 8) & 0xFF; w += 2;
        std::memcpy(w, "data", 4); w += 4;
        w[0] = data_chunk_size & 0xFF; w[1] = (data_chunk_size >> 8) & 0xFF;
        w[2] = (data_chunk_size >> 16) & 0xFF; w[3] = (data_chunk_size >> 24) & 0xFF;

        write_metadata_block(os, 0x03, swd_payload.data(), static_cast<int>(swd_payload.size()));
    }

    // --- EndOfMetaData block (Type 0) ---
    os.write("\x00\x00\x00\x00", 4); // type=0, size=0

    size_t audio_start_offset = os.tellp();
    size_t last_frame_start = 0;

    // Encode frames
    // Frame size for Fs250ms is exactly 250ms worth of samples
    int frame_samples = (sample_rate * 250) / 1000;
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
            int max_lpc = cfg.max_frame_lpc_mode;
            int costs[4];
            for (int m = 0; m <= max_lpc; m++) costs[m] = estimate_lpc_cost(c1.data(), current_frame_samples, m);
            lpc_mode_c1 = 0;
            for (int m = 1; m <= max_lpc; m++) if (costs[m] < costs[lpc_mode_c1]) lpc_mode_c1 = m;

            if (channels == 2) {
                for (int m = 0; m <= max_lpc; m++) costs[m] = estimate_lpc_cost(c2.data(), current_frame_samples, m);
                lpc_mode_c2 = 0;
                for (int m = 1; m <= max_lpc; m++) if (costs[m] < costs[lpc_mode_c2]) lpc_mode_c2 = m;
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
            encode_channel(d, current_frame_samples, bps, lpc, sample_rate, cfg, fw);
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
        last_frame_start = os.tellp();
        int frame_size = fw.get_position_bytes();
        int payload_size = frame_size - (header_bytes + 3);
        uint32_t frame_crc = takdecomp::compute_crc24(fw.get_data().data() + header_bytes + 3, payload_size);
        fw.write_bits((frame_crc >> 16) & 0xff, 8);
        fw.write_bits((frame_crc >> 8) & 0xff, 8);
        fw.write_bits(frame_crc & 0xff, 8);

        int frame_bytes = fw.get_position_bytes();
        
        if (remaining_samples - current_frame_samples <= 0) {
            size_t last_frame_end = last_frame_start + frame_bytes;
            int last_frame_size = frame_bytes;
            os.seekp(last_frame_md_offset);
            
            uint64_t lf_pos = last_frame_start - audio_start_offset;
            uint8_t lf_payload[8];
            lf_payload[0] = lf_pos & 0xFF;
            lf_payload[1] = (lf_pos >> 8) & 0xFF;
            lf_payload[2] = (lf_pos >> 16) & 0xFF;
            lf_payload[3] = (lf_pos >> 24) & 0xFF;
            lf_payload[4] = (lf_pos >> 32) & 0xFF;
            lf_payload[5] = last_frame_size & 0xFF;
            lf_payload[6] = (last_frame_size >> 8) & 0xFF;
            lf_payload[7] = (last_frame_size >> 16) & 0xFF;
            
            os.write(reinterpret_cast<const char*>(lf_payload), 8);
            uint32_t up_crc = takdecomp::compute_crc24(lf_payload, 8);
            uint8_t up_crc_be[3] = {
                static_cast<uint8_t>((up_crc >> 16) & 0xff),
                static_cast<uint8_t>((up_crc >> 8) & 0xff),
                static_cast<uint8_t>(up_crc & 0xff)
            };
            os.write(reinterpret_cast<char*>(up_crc_be), 3);
            
            // Seek back to where the last frame data should be written
            os.seekp(last_frame_start);
        }

        os.write(reinterpret_cast<const char*>(fw.get_data().data()), frame_bytes);
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
