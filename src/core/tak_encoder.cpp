#include "tak_encoder/encoder.hpp"
#include "tak_encoder/decorrelate.hpp"
#include "tak_decoder/constants.hpp"
#include "tak_decoder/tak_crc.hpp"
#include "tak_encoder/bitstream_writer.hpp"
#include "tak_encoder/wav_reader.hpp"
#include "tak_encoder/filter.hpp"
#include "tak_encoder/subframe.hpp"
#include "tak_encoder/md5.hpp"
#include "tak_encoder/ape_tag.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <algorithm>
#include <future>
#include <thread>
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
    os.write(reinterpret_cast<const char*>(size_le), 3);
    os.write(reinterpret_cast<const char*>(payload), payload_len);
    uint32_t crc = takdecomp::compute_crc24(payload, payload_len);
    const uint8_t crc_be[3] = {
        static_cast<uint8_t>((crc >> 16) & 0xff),
        static_cast<uint8_t>((crc >> 8) & 0xff),
        static_cast<uint8_t>(crc & 0xff)
    };
    os.write(reinterpret_cast<const char*>(crc_be), 3);
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
    int64_t total_samples = wav.data_size / block_align;
    if (cfg.ignore_header_size) {
        total_samples = 0;
    }

    os.write("tBaK", 4);
    size_t si_md_offset = os.tellp();

    auto write_si = [&](int64_t ts) {
        BitStreamWriter si_gb;
        takdecomp::CodecType codec = (channels > 2) ? takdecomp::CodecType::MultiChannel : takdecomp::CodecType::MonoStereo;
        si_gb.write_bits(static_cast<int>(codec), takdecomp::constants::ENCODER_CODEC_BITS);
        si_gb.write_bits(0, takdecomp::constants::ENCODER_PROFILE_BITS);
        
        int fsl = cfg.frame_size_limit;
        int frame_size_type = static_cast<int>(takdecomp::FrameSizeType::Fs250ms);
        if (fsl == 512) frame_size_type = 0;
        else if (fsl == 1024) frame_size_type = 1;
        else if (fsl == 2048) frame_size_type = 2;
        else if (fsl == 4096) frame_size_type = 3;
        else if (fsl == 8192) frame_size_type = 4;
        else if (fsl == 16384) frame_size_type = 5;
        
        si_gb.write_bits(frame_size_type, takdecomp::constants::SIZE_FRAME_DURATION_BITS);
        si_gb.write_bits64(ts, takdecomp::constants::SIZE_SAMPLES_NUM_BITS);
        si_gb.write_bits(0, takdecomp::constants::FORMAT_DATA_TYPE_BITS);
        si_gb.write_bits(sample_rate - takdecomp::constants::SAMPLE_RATE_MIN, takdecomp::constants::FORMAT_SAMPLE_RATE_BITS);
        si_gb.write_bits(bps - takdecomp::constants::BPS_MIN, takdecomp::constants::FORMAT_BPS_BITS);
        si_gb.write_bits(channels - takdecomp::constants::CHANNELS_MIN, takdecomp::constants::FORMAT_CHANNEL_BITS);
        si_gb.write_bit(0);
        si_gb.align_write_bits();
        return si_gb;
    };

    BitStreamWriter si_gb = write_si(total_samples);
    int si_len = si_gb.get_position_bytes();
    write_metadata_block(os, 0x01, si_gb.get_data().data(), si_len);

    os.write("\x07", 1);
    uint8_t lf_size_le[3] = { 11, 0, 0 };
    os.write(reinterpret_cast<char*>(lf_size_le), 3);
    size_t last_frame_md_offset = os.tellp();
    uint8_t lf_placeholder[8] = {0};
    os.write(reinterpret_cast<const char*>(lf_placeholder), 8);
    os.write("\x00\x00\x00", 3);

    uint8_t enc_info[4] = { 0x03, 0x03, 0x02, 0x00 };
    write_metadata_block(os, 0x04, enc_info, 4);

    {
        uint32_t fmt_chunk_size = wav.fmt_chunk.size();
        uint32_t wav_header_size = 20 + fmt_chunk_size + 8;
        uint32_t data_chunk_size = total_samples * channels * (bps / 8);
        uint32_t riff_size = 4 + 8 + fmt_chunk_size + 8 + data_chunk_size;
        
        std::vector<uint8_t> swd_payload(6 + wav_header_size);
        swd_payload[0] = wav_header_size & 0xFF; swd_payload[1] = (wav_header_size >> 8) & 0xFF;
        swd_payload[2] = (wav_header_size >> 16) & 0xFF; swd_payload[3] = (wav_header_size >> 24) & 0xFF;
        swd_payload[4] = 0; swd_payload[5] = 0;
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

    if (cfg.wave_metadata_mode > 0 && !wav.foreign_chunks.empty()) {
        std::vector<uint8_t> wm_payload;
        for (const auto& fc : wav.foreign_chunks) {
            wm_payload.insert(wm_payload.end(), fc.id, fc.id + 4);
            uint32_t size = fc.data.size();
            wm_payload.push_back(size & 0xFF);
            wm_payload.push_back((size >> 8) & 0xFF);
            wm_payload.push_back((size >> 16) & 0xFF);
            wm_payload.push_back((size >> 24) & 0xFF);
            wm_payload.insert(wm_payload.end(), fc.data.begin(), fc.data.end());
            if (size % 2 != 0) wm_payload.push_back(0);
        }
        write_metadata_block(os, 0x06, wm_payload.data(), static_cast<int>(wm_payload.size()));
    }

    os.write("\x00\x00\x00\x00", 4);

    size_t audio_start_offset = os.tellp();
    size_t last_frame_start = 0;
    
    int frame_samples = (sample_rate * 250) / 1000;
    if (cfg.frame_size_limit != 0 && cfg.frame_size_limit != 11025) {
        frame_samples = cfg.frame_size_limit;
    }
    
    int64_t remaining_samples = total_samples;
    int frame_num = 0;
    MD5 md5;

    struct FrameRes { std::vector<uint8_t> data; bool is_last; };

    auto process_frame = [channels, bps, sample_rate, cfg](std::vector<uint8_t> raw_bytes, int current_frame_samples, int f_num, bool is_last) -> FrameRes {
        Decorrelator decorr;
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

        BitStreamWriter fw;
        fw.write_bits(takdecomp::constants::FRAME_HEADER_SYNC_ID, takdecomp::constants::FRAME_HEADER_SYNC_ID_BITS);
        int flags = 0;
        if (is_last) flags |= takdecomp::constants::FRAME_FLAG_IS_LAST;
        fw.write_bits(flags, takdecomp::constants::FRAME_HEADER_FLAGS_BITS);
        fw.write_bits(f_num, takdecomp::constants::FRAME_HEADER_NO_BITS);
        if (is_last) {
            fw.write_bits(current_frame_samples - 1, takdecomp::constants::FRAME_HEADER_SAMPLE_COUNT_BITS);
            fw.write_bits(0, 2);
        }
        fw.align_write_bits();
        int header_bytes = fw.get_position_bytes();
        uint32_t header_crc = takdecomp::compute_crc24(fw.get_data().data(), header_bytes);
        fw.write_bits((header_crc >> 16) & 0xff, 8);
        fw.write_bits((header_crc >> 8) & 0xff, 8);
        fw.write_bits(header_crc & 0xff, 8);

        std::vector<int> lpc_mode(channels, 0);
        if (current_frame_samples >= 16) {
            int max_lpc = cfg.max_frame_lpc_mode;
            int costs[4];
            for (int ch = 0; ch < channels; ch++) {
                for (int m = 0; m <= max_lpc; m++) costs[m] = estimate_lpc_cost(c[ch].data(), current_frame_samples, m);
                for (int m = 1; m <= max_lpc; m++) if (costs[m] < costs[lpc_mode[ch]]) lpc_mode[ch] = m;
            }
        }

        for (int ch = 0; ch < channels; ch++) {
            inverse_lpc(c[ch].data(), lpc_mode[ch], current_frame_samples);
        }

        Decorrelator::DecorrelationResult dmode_res = {0, 0, 0, 0, {}};
        if (channels == 2) {
            dmode_res = decorr.apply_decorrelation(c[0].data(), c[1].data(), current_frame_samples);
        }

        if (channels > 2) fw.write_bit(0);
        
        for (int ch = 0; ch < channels; ch++) {
            encode_channel(c[ch].data(), current_frame_samples, bps, lpc_mode[ch], sample_rate, cfg, fw);
        }

        if (channels == 2) {
            fw.write_bit(0);
            fw.write_bits(dmode_res.mode, 3);
            if (dmode_res.mode >= 4 && dmode_res.mode <= 5) {
                if (dmode_res.shift > 0) { fw.write_bit(1); fw.write_bits(dmode_res.shift - 1, 4); }
                else { fw.write_bit(0); }
                fw.write_bits(dmode_res.factor & 0x3FF, 10);
            } else if (dmode_res.mode >= 6) {
                if (dmode_res.shift > 0) { fw.write_bit(1); fw.write_bits(dmode_res.shift - 1, 4); }
                else { fw.write_bit(0); }
                fw.write_bit(dmode_res.filter_order == 16 ? 1 : 0);
                fw.write_bit(1); fw.write_bit(0);
                for (int i = 0; i < dmode_res.filter_order; i += 4) {
                    int max_val = 0;
                    for (int j = 0; j < 4; j++) max_val = std::max(max_val, std::abs(dmode_res.filter[i + j]));
                    int code_size = 0;
                    while ((1 << code_size) <= max_val && code_size < 14) code_size++;
                    if (code_size > 0) code_size++;
                    if (code_size < 7) code_size = 7;
                    fw.write_bits(14 - code_size, 3);
                    for (int j = 0; j < 4; j++) {
                        fw.write_bits(dmode_res.filter[i + j] & ((1 << code_size) - 1), code_size);
                    }
                }
            }
        }

        fw.align_write_bits();
        int frame_size = fw.get_position_bytes();
        int payload_size = frame_size - (header_bytes + 3);
        uint32_t frame_crc = takdecomp::compute_crc24(fw.get_data().data() + header_bytes + 3, payload_size);
        fw.write_bits((frame_crc >> 16) & 0xff, 8);
        fw.write_bits((frame_crc >> 8) & 0xff, 8);
        fw.write_bits(frame_crc & 0xff, 8);

        return { fw.get_data(), is_last };
    };

    int max_in_flight = std::max(1, cfg.threads * 2);
    std::vector<std::future<FrameRes>> futures;
    int64_t real_total_samples = 0;
    int last_frame_size = 0;

    while (true) {
        if (!cfg.ignore_header_size && remaining_samples <= 0) break;

        int current_frame_samples = std::min(static_cast<int64_t>(frame_samples), remaining_samples);
        if (cfg.ignore_header_size) current_frame_samples = frame_samples;

        std::vector<uint8_t> raw_bytes(current_frame_samples * block_align);
        is.read(reinterpret_cast<char*>(raw_bytes.data()), raw_bytes.size());
        int bytes_read = is.gcount();
        if (bytes_read == 0) break;

        if (bytes_read < static_cast<int>(raw_bytes.size())) {
            raw_bytes.resize(bytes_read);
            current_frame_samples = bytes_read / block_align;
        }

        if (cfg.write_md5) md5.update(raw_bytes.data(), raw_bytes.size());

        real_total_samples += current_frame_samples;
        if (!cfg.ignore_header_size) remaining_samples -= current_frame_samples;

        bool is_last = false;
        if (!cfg.ignore_header_size && remaining_samples <= 0) is_last = true;
        else if (cfg.ignore_header_size && bytes_read < frame_samples * block_align) is_last = true;

        if (cfg.threads > 1) {
            futures.push_back(std::async(std::launch::async, process_frame, raw_bytes, current_frame_samples, frame_num, is_last));
            
            while (futures.size() >= static_cast<size_t>(max_in_flight)) {
                auto res = futures.front().get();
                if (res.is_last) {
                    last_frame_start = os.tellp();
                    last_frame_size = res.data.size();
                }
                os.write(reinterpret_cast<const char*>(res.data.data()), res.data.size());
                futures.erase(futures.begin());
            }
        } else {
            auto res = process_frame(raw_bytes, current_frame_samples, frame_num, is_last);
            if (res.is_last) {
                last_frame_start = os.tellp();
                last_frame_size = res.data.size();
            }
            os.write(reinterpret_cast<const char*>(res.data.data()), res.data.size());
        }

        if (progress) {
            progress(real_total_samples, cfg.ignore_header_size ? real_total_samples : total_samples);
        }

        frame_num++;
        if (is_last) break;
    }
    
    // Flush remaining futures
    for (auto& f : futures) {
        auto res = f.get();
        if (res.is_last) {
            last_frame_start = os.tellp();
            last_frame_size = res.data.size();
        }
        os.write(reinterpret_cast<const char*>(res.data.data()), res.data.size());
    }
    
    // Patch LastFrame metadata
    os.seekp(last_frame_md_offset);
    uint64_t lf_pos = last_frame_start - audio_start_offset;
    uint8_t lf_payload[8];
    lf_payload[0] = lf_pos & 0xFF; lf_payload[1] = (lf_pos >> 8) & 0xFF;
    lf_payload[2] = (lf_pos >> 16) & 0xFF; lf_payload[3] = (lf_pos >> 24) & 0xFF;
    lf_payload[4] = (lf_pos >> 32) & 0xFF;
    lf_payload[5] = last_frame_size & 0xFF; lf_payload[6] = (last_frame_size >> 8) & 0xFF;
    lf_payload[7] = (last_frame_size >> 16) & 0xFF;
    os.write(reinterpret_cast<const char*>(lf_payload), 8);
    uint32_t lf_crc = takdecomp::compute_crc24(lf_payload, 8);
    uint8_t crc_bytes[3];
    crc_bytes[0] = (lf_crc >> 16) & 0xFF; crc_bytes[1] = (lf_crc >> 8) & 0xFF; crc_bytes[2] = lf_crc & 0xFF;
    os.write(reinterpret_cast<const char*>(crc_bytes), 3);
    
    // Patch StreamInfo if we didn't know total_samples
    if (cfg.ignore_header_size) {
        os.seekp(si_md_offset);
        BitStreamWriter new_si_gb = write_si(real_total_samples);
        os.write(reinterpret_cast<const char*>(new_si_gb.get_data().data()), new_si_gb.get_position_bytes());
    }

    os.seekp(0, std::ios::end);

    if (progress) {
        progress(real_total_samples, real_total_samples);
    }
    
    if (cfg.write_md5) md5.finalize();
    
    if (cfg.write_ape_tag && !cfg.ape_tags.empty()) {
        ApeTagWriter ape;
        for (const auto& pair : cfg.ape_tags) ape.add_item(pair.first, pair.second);
        std::vector<uint8_t> ape_data = ape.generate();
        if (!ape_data.empty()) os.write(reinterpret_cast<const char*>(ape_data.data()), ape_data.size());
    }

    return EncodeResult{ cfg.write_md5 ? md5.to_string() : "" };
}

} // namespace takenc
