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
#include <bit>

namespace takenc {
    // Helper: write a metadata block (type byte, 3-byte LE size, payload)
    static void write_metadata_block(std::ostream &os, int tag, const uint8_t *payload, int payload_size) {
        uint8_t header[4];
        header[0] = tag;
        header[1] = payload_size & 0xFF;
        header[2] = (payload_size >> 8) & 0xFF;
        header[3] = (payload_size >> 16) & 0xFF;
        os.write(reinterpret_cast<const char *>(header), 4);
        if (payload_size > 0 && payload != nullptr) {
            os.write(reinterpret_cast<const char *>(payload), payload_size);
        }
    }

    EncodeResult Encoder::encode_file(const char *wav_path, const char *tak_path, const EncoderConfig &cfg,
                                      const ProgressCallback& progress) {
        std::ifstream is(wav_path, std::ios::binary);
        if (!is) throw std::runtime_error("Could not open WAV file");
        std::ofstream os(tak_path, std::ios::binary);
        if (!os) throw std::runtime_error("Could not open TAK file for writing");
        return encode_stream(is, os, cfg, progress);
    }

    EncodeResult Encoder::encode_stream(std::istream &is, std::ostream &os, const EncoderConfig &cfg,
                                        const ProgressCallback& progress) {
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

        int channel_mask = 0;
        if (wav.fmt_chunk.size() >= 40 && wav.fmt_chunk[0] == 0xFE && wav.fmt_chunk[1] == 0xFF) {
            // Extensible format
            channel_mask = wav.fmt_chunk[20] | (wav.fmt_chunk[21] << 8) | (wav.fmt_chunk[22] << 16) | (wav.fmt_chunk[23] << 24);
        }

        auto write_format_info = [&](BitStreamWriter& fw) {
            if (channels > 2) {
                fw.write_bits(1, 1); // has_extended_format = 1
                fw.write_bits(bps - 1, takdecomp::constants::FORMAT_VALID_BITS);
                if (channel_mask != 0) {
                    fw.write_bits(1, 1); // has_channel_layout = 1
                    
                    constexpr int64_t tak_channel_layouts[19] = {
                        0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
                        0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000,
                        0x8000, 0x10000, 0x20000
                    };
                    
                    int written_channels = 0;
                    printf("Writing channel_mask: %08x\n", channel_mask);
                    for (int i = 1; i < 19; i++) {
                        if (channel_mask & tak_channel_layouts[i]) {
                            fw.write_bits(i, takdecomp::constants::FORMAT_CH_LAYOUT_BITS);
                            printf("Wrote layout index %d\n", i);
                            written_channels++;
                        }
                    }
                    // If channel_mask has fewer bits than channels, pad with 0s
                    while (written_channels < channels) {
                        fw.write_bits(0, takdecomp::constants::FORMAT_CH_LAYOUT_BITS);
                        written_channels++;
                    }
                } else {
                    fw.write_bits(0, 1); // has_channel_layout = 0
                }
            } else {
                fw.write_bits(0, 1); // has_extended_format = 0
            }
        };
        int frame_samples = (sample_rate * 250) / 1000;
        int frame_size_type = static_cast<int>(takdecomp::FrameSizeType::Fs250ms);

        if (frame_samples > 16384) {
            frame_size_type = static_cast<int>(takdecomp::FrameSizeType::Fs8192);
            frame_samples = 8192;
        }

        auto write_si = [&](int64_t ts, int frame_size_type_val) {
            BitStreamWriter si_gb;
            takdecomp::CodecType codec = (channels > 2)
                                             ? takdecomp::CodecType::MultiChannel
                                             : takdecomp::CodecType::MonoStereo;
            si_gb.write_bits(static_cast<int>(codec), takdecomp::constants::ENCODER_CODEC_BITS);
            si_gb.write_bits(0, takdecomp::constants::ENCODER_PROFILE_BITS);
            
            si_gb.write_bits(frame_size_type_val, takdecomp::constants::SIZE_FRAME_DURATION_BITS);
            si_gb.write_bits64(ts, takdecomp::constants::SIZE_SAMPLES_NUM_BITS);
            si_gb.write_bits(0, takdecomp::constants::FORMAT_DATA_TYPE_BITS);
            si_gb.write_bits(sample_rate - takdecomp::constants::SAMPLE_RATE_MIN,
                             takdecomp::constants::FORMAT_SAMPLE_RATE_BITS);
            si_gb.write_bits(bps - takdecomp::constants::BPS_MIN, takdecomp::constants::FORMAT_BPS_BITS);
            si_gb.write_bits(channels - takdecomp::constants::CHANNELS_MIN, takdecomp::constants::FORMAT_CHANNEL_BITS);
            write_format_info(si_gb);
            si_gb.align_write_bits();
            return si_gb;
        };

        BitStreamWriter si_gb = write_si(total_samples, frame_size_type);
        std::vector<uint8_t> si_payload = si_gb.get_data();
        uint32_t si_crc = takdecomp::compute_crc24(si_payload.data(), si_payload.size());
        si_payload.push_back((si_crc >> 16) & 0xFF);
        si_payload.push_back((si_crc >> 8) & 0xFF);
        si_payload.push_back(si_crc & 0xFF);
        write_metadata_block(os, 0x01, si_payload.data(), si_payload.size());

        size_t last_frame_md_offset = os.tellp();
        last_frame_md_offset += 4; // Skip type (1 byte) and size (3 bytes)
        uint8_t lf_placeholder[11] = {0};
        write_metadata_block(os, 0x07, lf_placeholder, 11);

        uint8_t enc_info[7] = {0x03, 0x03, 0x02, 0x00, 0, 0, 0};
        uint32_t enc_crc = takdecomp::compute_crc24(enc_info, 4);
        enc_info[4] = (enc_crc >> 16) & 0xFF;
        enc_info[5] = (enc_crc >> 8) & 0xFF;
        enc_info[6] = enc_crc & 0xFF;
        write_metadata_block(os, 0x04, enc_info, 7); {
            uint32_t fmt_chunk_size = wav.fmt_chunk.size();
            uint32_t wav_header_size = 20 + fmt_chunk_size + 8;
            uint32_t data_chunk_size = total_samples * channels * (bps / 8);
            uint32_t riff_size = 4 + 8 + fmt_chunk_size + 8 + data_chunk_size;

            std::vector<uint8_t> swd_payload(6 + wav_header_size);
            swd_payload[0] = wav_header_size & 0xFF;
            swd_payload[1] = (wav_header_size >> 8) & 0xFF;
            swd_payload[2] = (wav_header_size >> 16) & 0xFF;
            swd_payload[3] = (wav_header_size >> 24) & 0xFF;
            swd_payload[4] = 0;
            swd_payload[5] = 0;
            uint8_t *w = swd_payload.data() + 6;
            std::memcpy(w, "RIFF", 4);
            w += 4;
            w[0] = riff_size & 0xFF;
            w[1] = (riff_size >> 8) & 0xFF;
            w[2] = (riff_size >> 16) & 0xFF;
            w[3] = (riff_size >> 24) & 0xFF;
            w += 4;
            std::memcpy(w, "WAVE", 4);
            w += 4;
            std::memcpy(w, "fmt ", 4);
            w += 4;
            w[0] = fmt_chunk_size & 0xFF;
            w[1] = (fmt_chunk_size >> 8) & 0xFF;
            w[2] = (fmt_chunk_size >> 16) & 0xFF;
            w[3] = (fmt_chunk_size >> 24) & 0xFF;
            w += 4;
            std::memcpy(w, wav.fmt_chunk.data(), fmt_chunk_size);
            w += fmt_chunk_size;
            std::memcpy(w, "data", 4);
            w += 4;
            w[0] = data_chunk_size & 0xFF;
            w[1] = (data_chunk_size >> 8) & 0xFF;
            w[2] = (data_chunk_size >> 16) & 0xFF;
            w[3] = (data_chunk_size >> 24) & 0xFF;

            uint32_t swd_crc = takdecomp::compute_crc24(swd_payload.data(), swd_payload.size());
            swd_payload.push_back((swd_crc >> 16) & 0xFF);
            swd_payload.push_back((swd_crc >> 8) & 0xFF);
            swd_payload.push_back(swd_crc & 0xFF);
            write_metadata_block(os, 0x03, swd_payload.data(), static_cast<int>(swd_payload.size()));
        }

        // 0x06 in TAK is MD5, which causes ffmpeg to fail if size != 19
        size_t md5_md_offset = 0;
        if (cfg.write_md5) {
            md5_md_offset = os.tellp();
            uint8_t md5_placeholder[23] = {0};
            md5_placeholder[0] = 0x06; // Type MD5
            md5_placeholder[1] = 19;   // Size LE (16 + 3)
            os.write(reinterpret_cast<const char *>(md5_placeholder), 23);
        }

        os.write("\x00\x00\x00\x00", 4);

        size_t audio_start_offset = os.tellp();
        size_t last_frame_start = 0;


        int64_t remaining_samples = total_samples;
        int frame_num = 0;
        MD5 md5;

        struct FrameRes {
            std::vector<uint8_t> data;
            bool is_last = false;
        };

        uint32_t header_total_samples = 0;
        if (wav.data_size > 0 && bps > 0 && channels > 0) {
            header_total_samples = wav.data_size / (channels * (bps / 8));
        }

        auto process_frame = [channels, bps, sample_rate, cfg, header_total_samples, frame_size_type, &write_format_info](const std::vector<uint8_t> &raw_bytes,
                                                                                                                          int current_frame_samples, int f_num,
                                                                                                                          bool is_last) -> FrameRes {
            std::vector<std::vector<int32_t> > c(channels, std::vector<int32_t>(current_frame_samples));
            int byte_idx = 0;
            int const bytes_per_sample = bps / 8;
            for (int i = 0; i < current_frame_samples; i++) {
                for (int ch = 0; ch < channels; ch++) {
                    if (bps == 8) {
                        int32_t val = static_cast<int32_t>(raw_bytes[byte_idx]) - 128;
                        c[ch][i] = val;
                    } else if (bps == 16) {
                        int16_t val = raw_bytes[byte_idx] | (raw_bytes[byte_idx + 1] << 8);
                        c[ch][i] = val;
                    } else if (bps == 24) {
                        int32_t val = raw_bytes[byte_idx] | (raw_bytes[byte_idx + 1] << 8) | (
                                          raw_bytes[byte_idx + 2] << 16);
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
            if (f_num == 0) flags |= takdecomp::constants::FRAME_FLAG_HAS_INFO;
            fw.write_bits(flags, takdecomp::constants::FRAME_HEADER_FLAGS_BITS);
            fw.write_bits(f_num, takdecomp::constants::FRAME_HEADER_NO_BITS);
            if (is_last) {
                fw.write_bits(current_frame_samples - 1, takdecomp::constants::FRAME_HEADER_SAMPLE_COUNT_BITS);
                fw.write_bits(0, 2);
            }
            if (f_num == 0) {
                fw.write_bits(static_cast<int>(channels > 2 ? takdecomp::CodecType::MultiChannel : takdecomp::CodecType::MonoStereo), takdecomp::constants::ENCODER_CODEC_BITS);
                fw.write_bits(0, takdecomp::constants::ENCODER_PROFILE_BITS);
                fw.write_bits(frame_size_type, takdecomp::constants::SIZE_FRAME_DURATION_BITS);
                fw.write_bits64(header_total_samples, 35);
                fw.write_bits(0, takdecomp::constants::FORMAT_DATA_TYPE_BITS); // PCM
                fw.write_bits(sample_rate - takdecomp::constants::SAMPLE_RATE_MIN, takdecomp::constants::FORMAT_SAMPLE_RATE_BITS);
                fw.write_bits(bps - takdecomp::constants::BPS_MIN, takdecomp::constants::FORMAT_BPS_BITS);
                fw.write_bits(channels - takdecomp::constants::CHANNELS_MIN, takdecomp::constants::FORMAT_CHANNEL_BITS);
                write_format_info(fw);
                fw.write_bits(0, 6);
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
                    for (int m = 0; m <= max_lpc; m++) costs[m] = estimate_lpc_cost(
                                                           c[ch].data(), current_frame_samples, m);
                    for (int m = 1; m <= max_lpc; m++) if (costs[m] < costs[lpc_mode[ch]]) lpc_mode[ch] = m;
                }
            }

            for (int ch = 0; ch < channels; ch++) {
                inverse_lpc(c[ch].data(), lpc_mode[ch], current_frame_samples);
            }

            Decorrelator::DecorrelationResult dmode_res = {0, 0, 0, 0, {}};
            if (channels == 2) {
                dmode_res = takenc::Decorrelator::apply_decorrelation(c[0].data(), c[1].data(), current_frame_samples);
            }

            if (current_frame_samples < 16) {
                for (int ch = 0; ch < channels; ch++) {
                    for (int i = 0; i < current_frame_samples; i++) {
                        fw.write_bits(c[ch][i], bps);
                    }
                }
            } else {
                if (channels > 2) fw.write_bit(0);

                for (int ch = 0; ch < channels; ch++) {
//            printf("dmode=%d\n", dmode_res.mode);
//            printf("c[0][0]=%d c[0][1]=%d c[0][2]=%d c[0][3]=%d c[0][4]=%d c[0][5]=%d c[0][6]=%d c[0][7]=%d \n", c[0][0], c[0][1], c[0][2], c[0][3], c[0][4], c[0][5], c[0][6], c[0][7]);
//            printf("c[1][0]=%d c[1][1]=%d c[1][2]=%d c[1][3]=%d c[1][4]=%d c[1][5]=%d c[1][6]=%d c[1][7]=%d \n", c[1][0], c[1][1], c[1][2], c[1][3], c[1][4], c[1][5], c[1][6], c[1][7]);
                    encode_channel(c[ch].data(), current_frame_samples, bps, lpc_mode[ch], sample_rate, cfg, fw);
                }

                if (channels == 2) {
                    fw.write_bit(0);
                    fw.write_bits(dmode_res.mode, 3);
                    if (dmode_res.mode >= 4 && dmode_res.mode <= 5) {
                        if (dmode_res.shift > 0) {
                            fw.write_bit(1);
                            fw.write_bits(dmode_res.shift - 1, 4);
                        } else { fw.write_bit(0); }
                        fw.write_bits(dmode_res.factor & 0x3FF, 10);
                    } else if (dmode_res.mode >= 6) {
                        if (dmode_res.shift > 0) {
                            fw.write_bit(1);
                            fw.write_bits(dmode_res.shift - 1, 4);
                        } else { fw.write_bit(0); }
                        fw.write_bit(dmode_res.filter_order == 16 ? 1 : 0);
                        fw.write_bit(1); // dval1
                        fw.write_bit(1); // dval2
                        for (int i = 0; i < dmode_res.filter_order; i += 4) {
                            int max_val = 0;
                            for (int j = 0; j < 4; j++) max_val = std::max(max_val, std::abs(dmode_res.filter[i + j]));
                            int code_size = std::min(14, std::bit_width(static_cast<uint32_t>(max_val)));
                            if (code_size > 0) code_size++;
                            if (code_size < 7) code_size = 7;
                            fw.write_bits(14 - code_size, 3);
                            for (int j = 0; j < 4; j++) {
                                fw.write_bits(dmode_res.filter[i + j] & ((1 << code_size) - 1), code_size);
                            }
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

            return {fw.get_data(), is_last};
        };

        int max_in_flight = std::max(1, cfg.threads * 2);
        std::vector<std::future<FrameRes> > futures;
        int64_t real_total_samples = 0;
        int last_frame_size = 0;

        while (true) {
            if (!cfg.ignore_header_size && remaining_samples <= 0) break;

            int current_frame_samples = std::min(static_cast<int64_t>(frame_samples), remaining_samples);
            if (cfg.ignore_header_size) current_frame_samples = frame_samples;

            std::vector<uint8_t> raw_bytes(current_frame_samples * block_align);
            is.read(reinterpret_cast<char *>(raw_bytes.data()), raw_bytes.size());
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
                futures.push_back(std::async(std::launch::async, process_frame, raw_bytes, current_frame_samples,
                                             frame_num, is_last));

                while (futures.size() >= static_cast<size_t>(max_in_flight)) {
                    auto res = futures.front().get();
                    if (res.is_last) {
                        last_frame_start = os.tellp();
                        last_frame_size = res.data.size();
                    }
                    os.write(reinterpret_cast<const char *>(res.data.data()), res.data.size());
                    futures.erase(futures.begin());
                }
            } else {
                auto res = process_frame(raw_bytes, current_frame_samples, frame_num, is_last);
                if (res.is_last) {
                    last_frame_start = os.tellp();
                    last_frame_size = res.data.size();
                }
                os.write(reinterpret_cast<const char *>(res.data.data()), res.data.size());
            }

            if (progress) {
                progress(real_total_samples, cfg.ignore_header_size ? real_total_samples : total_samples);
            }

            frame_num++;
            if (is_last) break;
        }

        // Flush remaining futures
        for (auto &f: futures) {
            auto res = f.get();
            if (res.is_last) {
                last_frame_start = os.tellp();
                last_frame_size = res.data.size();
            }
            os.write(reinterpret_cast<const char *>(res.data.data()), res.data.size());
        }

        // Patch LastFrame metadata
        if (last_frame_md_offset > 0) {
            os.seekp(last_frame_md_offset);
            uint8_t lf_data[11];
            uint64_t sample_start = last_frame_start - audio_start_offset;
            lf_data[0] = sample_start & 0xFF;
            lf_data[1] = (sample_start >> 8) & 0xFF;
            lf_data[2] = (sample_start >> 16) & 0xFF;
            lf_data[3] = (sample_start >> 24) & 0xFF;
            lf_data[4] = (sample_start >> 32) & 0xFF;
            lf_data[5] = (last_frame_size) & 0xFF;
            lf_data[6] = ((last_frame_size) >> 8) & 0xFF;
            lf_data[7] = ((last_frame_size) >> 16) & 0xFF;
            
            uint32_t lf_crc = takdecomp::compute_crc24(lf_data, 8);
            lf_data[8] = (lf_crc >> 16) & 0xFF;
            lf_data[9] = (lf_crc >> 8) & 0xFF;
            lf_data[10] = lf_crc & 0xFF;
            os.write(reinterpret_cast<const char *>(lf_data), 11);
        }

        // Patch StreamInfo if we didn't know total_samples
        if (cfg.ignore_header_size) {
            os.seekp(si_md_offset + 4);
            BitStreamWriter new_si_gb = write_si(real_total_samples, frame_size_type);
            std::vector<uint8_t> new_si_payload = new_si_gb.get_data();
            uint32_t new_si_crc = takdecomp::compute_crc24(new_si_payload.data(), new_si_payload.size());
            new_si_payload.push_back((new_si_crc >> 16) & 0xFF);
            new_si_payload.push_back((new_si_crc >> 8) & 0xFF);
            new_si_payload.push_back(new_si_crc & 0xFF);
            os.write(reinterpret_cast<const char *>(new_si_payload.data()), new_si_payload.size());
        }

        os.seekp(0, std::ios::end);

        if (progress) {
            progress(real_total_samples, real_total_samples);
        }

        if (cfg.write_md5) {
            md5.finalize();
            if (md5_md_offset > 0) {
                os.seekp(md5_md_offset + 4);
                auto digest = md5.digest();
                os.write(reinterpret_cast<const char *>(digest.data()), 16);
                uint32_t md5_crc = takdecomp::compute_crc24(digest.data(), 16);
                const uint8_t crc_be[3] = {
                    static_cast<uint8_t>((md5_crc >> 16) & 0xff),
                    static_cast<uint8_t>((md5_crc >> 8) & 0xff),
                    static_cast<uint8_t>(md5_crc & 0xff)
                };
                os.write(reinterpret_cast<const char *>(crc_be), 3);
                os.seekp(0, std::ios::end);
            }
        }

        if (cfg.write_ape_tag && !cfg.ape_tags.empty()) {
            ApeTagWriter ape;
            for (const auto &pair: cfg.ape_tags) ape.add_item(pair.first, pair.second);
            std::vector<uint8_t> ape_data = ape.generate();
            if (!ape_data.empty()) os.write(reinterpret_cast<const char *>(ape_data.data()), ape_data.size());
        }

        return EncodeResult{cfg.write_md5 ? md5.to_string() : ""};
    }
} // namespace takenc
