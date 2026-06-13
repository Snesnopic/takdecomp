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
    const uint8_t size_le[3] = {
        static_cast<uint8_t>(block_size & 0xff),
        static_cast<uint8_t>((block_size >> 8) & 0xff),
        static_cast<uint8_t>((block_size >> 16) & 0xff)
    };
    os.write(reinterpret_cast<char*>(size_le), 3);
    os.write(reinterpret_cast<const char*>(payload), payload_len);
    uint32_t crc = takdecomp::compute_crc24(payload, payload_len);
    const uint8_t crc_be[3] = {
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
    takdecomp::CodecType codec = (channels > 2) ? takdecomp::CodecType::MultiChannel : takdecomp::CodecType::MonoStereo;
    si_gb.write_bits(static_cast<int>(codec),
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
        uint32_t fmt_chunk_size = wav.fmt_chunk.size();
        uint32_t wav_header_size = 20 + fmt_chunk_size + 8; // RIFF(12) + fmt(8+fmt_chunk_size) + data(8)
        uint32_t data_chunk_size = total_samples * channels * (bps / 8);
        uint32_t riff_size = 4 + 8 + fmt_chunk_size + 8 + data_chunk_size; // WAVE + fmt header + fmt data + data header + data size
        
        std::vector<uint8_t> swd_payload(6 + wav_header_size);
        // First 4 bytes: WAV header size
        swd_payload[0] = wav_header_size & 0xFF;
        swd_payload[1] = (wav_header_size >> 8) & 0xFF;
        swd_payload[2] = (wav_header_size >> 16) & 0xFF;
        swd_payload[3] = (wav_header_size >> 24) & 0xFF;
        // Next 2 bytes: flags (0)
        swd_payload[4] = 0;
        swd_payload[5] = 0;
        // WAV header
        uint8_t* w = swd_payload.data() + 6;
        std::memcpy(w, "RIFF", 4); w += 4;
        w[0] = riff_size & 0xFF; w[1] = (riff_size >> 8) & 0xFF;
        w[2] = (riff_size >> 16) & 0xFF; w[3] = (riff_size >> 24) & 0xFF; w += 4;
        std::memcpy(w, "WAVE", 4); w += 4;
        std::memcpy(w, "fmt ", 4); w += 4;
        w[0] = fmt_chunk_size & 0xFF; w[1] = (fmt_chunk_size >> 8) & 0xFF;
        w[2] = (fmt_chunk_size >> 16) & 0xFF; w[3] = (fmt_chunk_size >> 24) & 0xFF; w += 4;
        std::memcpy(w, wav.fmt_chunk.data(), fmt_chunk_size); w += fmt_chunk_size;
        
        std::memcpy(w, "data", 4); w += 4;
        w[0] = data_chunk_size & 0xFF; w[1] = (data_chunk_size >> 8) & 0xFF;
        w[2] = (data_chunk_size >> 16) & 0xFF; w[3] = (data_chunk_size >> 24) & 0xFF;

        write_metadata_block(os, 0x03, swd_payload.data(), static_cast<int>(swd_payload.size()));
    }

    // --- EndOfMetaData block (Type 0) ---
    os.write("\x00\x00\x00\x00", 4); // type=0, size=0

    size_t audio_start_offset = os.tellp();
    

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

        std::vector<std::vector<int32_t>> c(channels, std::vector<int32_t>(current_frame_samples));

        int byte_idx = 0;
        int const bytes_per_sample = bps / 8;
        for (int i = 0; i < current_frame_samples; i++) {
            for (int ch = 0; ch < channels; ch++) {
                if (bps == 16) {
                    int16_t val = raw_bytes[byte_idx] | (raw_bytes[byte_idx+1] << 8);
                    c[ch][i] = val;
                } else if (bps == 24) {
                    int32_t val = raw_bytes[byte_idx] | (raw_bytes[byte_idx+1] << 8) | (raw_bytes[byte_idx+2] << 16);
                    if (val & 0x800000) val |= 0xFF000000;
                    c[ch][i] = val;
                }
                byte_idx += bytes_per_sample;
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
        std::vector<int> lpc_mode(channels, 0);
        if (current_frame_samples >= 16) {
            int max_lpc = cfg.max_frame_lpc_mode;
            int costs[4];
            for (int ch = 0; ch < channels; ch++) {
                for (int m = 0; m <= max_lpc; m++) costs[m] = estimate_lpc_cost(c[ch].data(), current_frame_samples, m);
                for (int m = 1; m <= max_lpc; m++) if (costs[m] < costs[lpc_mode[ch]]) lpc_mode[ch] = m;
            }
        }

        // Apply inverse LPC
        for (int ch = 0; ch < channels; ch++) {
            inverse_lpc(c[ch].data(), lpc_mode[ch], current_frame_samples);
        }

        // Apply inverse decorrelation (only for stereo currently)
        Decorrelator::DecorrelationResult dmode_res = {0, 0, 0, 0, {}};
        if (channels == 2) {
            dmode_res = decorr.apply_decorrelation(c[0].data(), c[1].data(), current_frame_samples);
        }

        // Multichannel decorrelation info is written BEFORE channels
        if (channels > 2) {
            fw.write_bit(0); // custom routing map present? 0 = false (no decorrelation)
        }

        // Encode channels
        
        for (int ch = 0; ch < channels; ch++) {
            encode_channel(c[ch].data(), current_frame_samples, bps, lpc_mode[ch], sample_rate, cfg, fw);
        }

        // Stereo decorrelation info is written AFTER channels
        if (channels == 2) {
            fw.write_bit(0); // nb_subframes - 1
            fw.write_bits(dmode_res.mode, 3);
            if (dmode_res.mode >= 4 && dmode_res.mode <= 5) {
                if (dmode_res.shift > 0) {
                    fw.write_bit(1);
                    fw.write_bits(dmode_res.shift - 1, 4);
                } else {
                    fw.write_bit(0);
                }
                fw.write_bits(dmode_res.factor & 0x3FF, 10);
            } else if (dmode_res.mode >= 6) {
                if (dmode_res.shift > 0) {
                    fw.write_bit(1);
                    fw.write_bits(dmode_res.shift - 1, 4);
                } else {
                    fw.write_bit(0);
                }
                fw.write_bit(dmode_res.filter_order == 16 ? 1 : 0);
                fw.write_bit(1); // dval1
                fw.write_bit(0); // dval2
                
                for (int i = 0; i < dmode_res.filter_order; i += 4) {
                    int max_val = 0;
                    for (int j = 0; j < 4; j++) max_val = std::max(max_val, std::abs(dmode_res.filter[i + j]));
                    int code_size = 0;
                    while ((1 << code_size) <= max_val && code_size < 14) code_size++;
                    if (code_size > 0) code_size++; // sign bit
                    if (code_size < 7) code_size = 7;
                    fw.write_bits(14 - code_size, 3);
                    for (int j = 0; j < 4; j++) {
                        fw.write_bits(dmode_res.filter[i + j] & ((1 << code_size) - 1), code_size);
                    }
                }
            }
        }

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
