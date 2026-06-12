#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <iomanip>

using namespace takdecomp;

uint32_t read_le32(std::ifstream& is) {
    uint8_t buf[4];
    is.read(reinterpret_cast<char*>(buf), 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

uint32_t read_le24(std::ifstream& is) {
    uint8_t buf[3];
    is.read(reinterpret_cast<char*>(buf), 3);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16);
}

uint8_t read_u8(std::ifstream& is) {
    uint8_t buf;
    is.read(reinterpret_cast<char*>(&buf), 1);
    return buf;
}

void write_wav_header(std::ofstream& os, int sample_rate, int channels, int bps, int total_samples) {
    os.write("RIFF", 4);
    uint32_t data_size = total_samples * channels * (bps / 8);
    uint32_t file_size = 36 + data_size;
    os.write(reinterpret_cast<const char*>(&file_size), 4);
    os.write("WAVE", 4);
    os.write("fmt ", 4);
    uint32_t fmt_size = 16;
    os.write(reinterpret_cast<const char*>(&fmt_size), 4);
    uint16_t audio_format = 1; // PCM
    os.write(reinterpret_cast<const char*>(&audio_format), 2);
    uint16_t num_channels = channels;
    os.write(reinterpret_cast<const char*>(&num_channels), 2);
    uint32_t sr = sample_rate;
    os.write(reinterpret_cast<const char*>(&sr), 4);
    uint32_t byte_rate = sample_rate * channels * (bps / 8);
    os.write(reinterpret_cast<const char*>(&byte_rate), 4);
    uint16_t block_align = channels * (bps / 8);
    os.write(reinterpret_cast<const char*>(&block_align), 2);
    uint16_t bits_per_sample = bps;
    os.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    os.write("data", 4);
    os.write(reinterpret_cast<const char*>(&data_size), 4);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.tak> <output.wav>\n";
        return 1;
    }

    std::ifstream is(argv[1], std::ios::binary);
    if (!is) {
        std::cerr << "Failed to open input file.\n";
        return 1;
    }

    uint32_t magic = read_le32(is);
    if (magic != 0x4B614274) { // "tBaK" in LE
        std::cerr << "Not a valid TAK file.\n";
        return 1;
    }

    StreamInfo stream_info;
    bool has_stream_info = false;
    uint64_t data_end = 0;
    bool has_data_end = false;

    // Demux metadata
    while (is) {
        uint8_t type_byte = read_u8(is);
        MetaDataType type = static_cast<MetaDataType>(type_byte & 0x7f);
        uint32_t size = read_le24(is);

        if (type == MetaDataType::End) {
            uint64_t curpos = is.tellg();
            if (!has_data_end) data_end = curpos; // Will compute properly below
            break;
        }

        if (size <= 3) {
            std::cerr << "Invalid metadata size\n";
            return 1;
        }

        std::vector<uint8_t> buffer(size - 3);
        is.read(reinterpret_cast<char*>(buffer.data()), size - 3);
        is.seekg(3, std::ios::cur); // Skip CRC

        if (type == MetaDataType::StreamInfo) {
            Decoder dec;
            BitStreamReader gb(buffer);
            stream_info = dec.parse_streaminfo(gb);
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

    // Write dummy WAV header, will update later
    write_wav_header(os, stream_info.sample_rate, stream_info.channels, stream_info.bps, stream_info.samples);

    Decoder decoder;
    int total_samples_written = 0;
    
    // Read remaining file into memory
    size_t current_pos = is.tellg();
    is.seekg(0, std::ios::end);
    size_t file_size = is.tellg();
    if (data_end == 0 || !has_data_end || data_end > file_size) {
        data_end = file_size;
    }
    
    size_t data_len = data_end - current_pos;
    std::vector<uint8_t> file_data(data_len);
    is.seekg(current_pos, std::ios::beg);
    is.read(reinterpret_cast<char*>(file_data.data()), data_len);
    
    size_t pos = 0;
    while (pos < file_data.size() - 2) {
        // Find sync 0xA0FF
        if (file_data[pos] == 0xFF && file_data[pos+1] == 0xA0) {
            // Find next sync word to determine frame size
            size_t next_sync = pos + 2;
            while (next_sync < file_data.size() - 1) {
                if (file_data[next_sync] == 0xFF && file_data[next_sync+1] == 0xA0) {
                    // Validate if it's a real frame header by reading the first few bits
                    try {
                        std::span<const uint8_t> check_span(file_data.data() + next_sync, file_data.size() - next_sync);
                        BitStreamReader check_gb(check_span);
                        StreamInfo dummy_info;
                        decoder.decode_frame_header(check_gb, dummy_info);
                        // If it succeeds without throwing, it's highly likely a valid sync word
                        break;
                    } catch (...) {
                        // Not a valid sync word, continue searching
                    }
                }
                next_sync++;
            }
            
            size_t frame_size = next_sync - pos;
            
            // Pad the frame with 64 bytes of zeroes for the bitstream reader to safely over-read
            std::vector<uint8_t> padded_frame(frame_size + 64, 0);
            std::memcpy(padded_frame.data(), file_data.data() + pos, frame_size);
            
            try {
                std::span<const uint8_t> frame_span(padded_frame);
                std::vector<std::vector<int32_t>> decoded_channels;
                
                decoder.decode_frame(frame_span, stream_info, decoded_channels);
                
                int nb_samples = decoded_channels[0].size();
                int channels = stream_info.channels;
                int bytes_per_sample = stream_info.bps / 8;
                
                for (int s = 0; s < nb_samples; ++s) {
                    for (int c = 0; c < channels; ++c) {
                        int32_t sample = decoded_channels[c][s];
                        if (stream_info.bps == 8) {
                            uint8_t out = static_cast<uint8_t>(sample + 0x80);
                            os.write(reinterpret_cast<char*>(&out), 1);
                        } else if (stream_info.bps == 16) {
                            int16_t out = static_cast<int16_t>(sample);
                            os.write(reinterpret_cast<char*>(&out), 2);
                        } else if (stream_info.bps == 24) {
                            os.write(reinterpret_cast<char*>(&sample), 3);
                        }
                    }
                }
                total_samples_written += nb_samples;
                
                pos = next_sync;
            } catch (const std::exception& e) {
                // If decode fails, it might be a false sync. Advance by 1 to search for next sync.
                std::cerr << "Frame decode failed at pos " << pos << ": " << e.what() << "\n";
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
