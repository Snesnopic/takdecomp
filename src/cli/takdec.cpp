#include "tak_decoder/constants.hpp"
#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <cstdint>
#include "tak_decoder/streaminfo.hpp"
#include <exception>
#include <iostream>
#include <fstream>
#include <span>
#include <vector>
#include <cstring>

using namespace takdecomp;

static uint32_t read_le32(std::ifstream &is) {
    uint8_t buf[4];
    is.read(reinterpret_cast<char *>(buf), 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static uint32_t read_le24(std::ifstream &is) {
    uint8_t buf[3];
    is.read(reinterpret_cast<char *>(buf), 3);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16);
}

static uint8_t read_u8(std::ifstream &is) {
    uint8_t buf;
    is.read(reinterpret_cast<char *>(&buf), 1);
    return buf;
}

static void write_wav_header(std::ofstream &os, const int sample_rate, const int channels, const int bps, const int total_samples) {
    os.write("RIFF", 4);
    const uint32_t data_size = total_samples * channels * (bps / 8);
    const uint32_t file_size = 36 + data_size;
    os.write(reinterpret_cast<const char *>(&file_size), 4);
    os.write("WAVE", 4);
    os.write("fmt ", 4);
    constexpr uint32_t fmt_size = 16;
    os.write(reinterpret_cast<const char *>(&fmt_size), 4);
    constexpr uint16_t audio_format = 1; // PCM
    os.write(reinterpret_cast<const char *>(&audio_format), 2);
    const uint16_t num_channels = channels;
    os.write(reinterpret_cast<const char *>(&num_channels), 2);
    const uint32_t sr = sample_rate;
    os.write(reinterpret_cast<const char *>(&sr), 4);
    const uint32_t byte_rate = sample_rate * channels * (bps / 8);
    os.write(reinterpret_cast<const char *>(&byte_rate), 4);
    const uint16_t block_align = channels * (bps / 8);
    os.write(reinterpret_cast<const char *>(&block_align), 2);
    const uint16_t bits_per_sample = bps;
    os.write(reinterpret_cast<const char *>(&bits_per_sample), 2);
    os.write("data", 4);
    os.write(reinterpret_cast<const char *>(&data_size), 4);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.tak> <output.wav>\n";
        return 1;
    }

    std::ifstream is(argv[1], std::ios::binary);
    if (!is) {
        std::cerr << "Failed to open input file.\n";
        return 1;
    }

    uint32_t const magic = read_le32(is);
    if (magic != 0x4B614274) {
        // "tBaK" in LE
        std::cerr << "Not a valid TAK file.\n";
        return 1;
    }

    StreamInfo stream_info;
    bool has_stream_info = false;
    uint64_t data_end = 0;
    bool has_data_end = false;

    // Demux metadata
    while (is) {
        uint8_t const type_byte = read_u8(is);
        auto const type = static_cast<MetaDataType>(type_byte & 0x7f);
        uint32_t const size = read_le24(is);

        if (type == MetaDataType::End) {
            uint64_t const curpos = is.tellg();
            if (!has_data_end) {
                data_end = curpos; // Will compute properly below
            }
            break;
        }

        std::vector<uint8_t> buffer(size);
        is.read(reinterpret_cast<char *>(buffer.data()), size);
        is.seekg(3, std::ios::cur); // Skip CRC

        if (type == MetaDataType::StreamInfo) {
            Decoder dec;
            BitStreamReader gb(buffer);
            stream_info = takdecomp::Decoder::parse_streaminfo(gb);
            has_stream_info = true;
        } else if (type == MetaDataType::LastFrame) {
            if (buffer.size() >= 8) {
                BitStreamReader gb(buffer);
                data_end = gb.get_bits64(40) + gb.get_bits(24);
                has_data_end = true;
            }
        }
    }

    if (!has_stream_info) {
        std::cerr << "No stream info found.\n";
        return 1;
    }

    std::ofstream os(argv[2], std::ios::binary);
    if (!os) {
        std::cerr << "Failed to open output file.\n";
        return 1;
    }

    // Write the WAV header (total samples is known from StreamInfo)
    write_wav_header(os, stream_info.sample_rate, stream_info.channels, stream_info.bps, stream_info.samples);

    Decoder decoder;
    std::size_t total_samples_written = 0;

    // Read remaining file into memory
    size_t const current_pos = is.tellg();
    is.seekg(0, std::ios::end);
    size_t const file_size = is.tellg();
    if (data_end == 0 || !has_data_end || data_end > file_size) {
        data_end = file_size;
    }

    size_t const data_len = data_end - current_pos;
    std::vector<uint8_t> file_data(data_len);
    is.seekg(current_pos, std::ios::beg);
    is.read(reinterpret_cast<char *>(file_data.data()), data_len);

    size_t pos = 0;
    while (pos < file_data.size() - 2) {
        // Find sync 0xA0FF
        if (file_data[pos] == 0xFF && file_data[pos + 1] == 0xA0) {
            // Find next sync word to determine frame size
            size_t next_sync = pos + 2;
            while (next_sync < file_data.size() - 1) {
                if (file_data[next_sync] == 0xFF && file_data[next_sync + 1] == 0xA0) {
                    // Validate if it's a real frame header by reading the first few bits
                    try {
                        std::span<const uint8_t> const check_span(file_data.data() + next_sync,
                                                                  file_data.size() - next_sync);
                        BitStreamReader check_gb(check_span);
                        StreamInfo dummy_info;
                        takdecomp::Decoder::decode_frame_header(check_gb, dummy_info);
                        // If it succeeds without throwing, it's highly likely a valid sync word
                        break;
                    } catch (const std::exception &e) {
                        // Not a valid sync word, continue searching
                    }
                }
                next_sync++;
            }

            size_t const frame_size = next_sync - pos;

            // Pad the frame with 64 bytes of zeroes for the bitstream reader to safely over-read
            std::vector<uint8_t> padded_frame(frame_size + 64, 0);
            std::memcpy(padded_frame.data(), file_data.data() + pos, frame_size);

            try {
                std::span<const uint8_t> const frame_span(padded_frame);
                std::vector<std::vector<int32_t> > decoded_channels;
                decoder.decode_frame(frame_span, stream_info, decoded_channels);
                const std::size_t nb_samples = decoded_channels.empty() ? 0 : decoded_channels[0].size();

                int const channels = stream_info.channels;

                for (std::size_t s = 0; s < nb_samples; ++s) {
                    for (int c = 0; c < channels; ++c) {
                        int32_t sample = decoded_channels[c][s];
                        if (stream_info.bps == 8) {
                            auto out = static_cast<uint8_t>(sample + 0x80);
                            os.write(reinterpret_cast<char *>(&out), 1);
                        } else if (stream_info.bps == 16) {
                            auto out = static_cast<int16_t>(sample);
                            os.write(reinterpret_cast<char *>(&out), 2);
                        } else if (stream_info.bps == 24) {
                            os.write(reinterpret_cast<char *>(&sample), 3);
                        }
                    }
                }
                total_samples_written += nb_samples;

                pos = next_sync;
            } catch (const std::exception &e) {
                std::cerr << "Decode error at pos " << pos << ": " << e.what() << "\n";
                // If decode fails, it might be a false sync. Advance by 1 to search for next sync.
                pos++;
            }
        } else {
            pos++;
        }
    }

    // Fix WAV header size
    os.seekp(0);
    write_wav_header(os, stream_info.sample_rate, stream_info.channels, stream_info.bps, total_samples_written);

    std::cout << "Successfully decoded TAK file to WAV.\n";
    std::cout << "Codec config: " << stream_info.sample_rate << "Hz, "
            << stream_info.channels << " channels, "
            << stream_info.bps << " bits per sample.\n";
    std::cout << "Decoded " << total_samples_written << " samples.\n";
    std::cout << "Output ready at " << argv[2] << "\n";

    return 0;
}
