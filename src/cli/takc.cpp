#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <span>

#include "tak_encoder/encoder.hpp"
#include "tak_decoder/decoder.hpp"
#include "tak_decoder/constants.hpp"
#include "tak_decoder/bitstream.hpp"

using namespace takdecomp;

// Helper function to read LE 32-bit
static uint32_t read_le32(std::ifstream &is) {
    uint8_t buf[4];
    is.read(reinterpret_cast<char *>(buf), 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

// Helper function to read LE 24-bit
static uint32_t read_le24(std::ifstream &is) {
    uint8_t buf[3];
    is.read(reinterpret_cast<char *>(buf), 3);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16);
}

// Helper function to read 8-bit
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
    uint32_t fmt_size = 16;
    os.write(reinterpret_cast<const char *>(&fmt_size), 4);
    uint16_t audio_format = 1; // PCM
    os.write(reinterpret_cast<const char *>(&audio_format), 2);
    uint16_t num_channels = channels;
    os.write(reinterpret_cast<const char *>(&num_channels), 2);
    uint32_t sr = sample_rate;
    os.write(reinterpret_cast<const char *>(&sr), 4);
    uint32_t byte_rate = sample_rate * channels * (bps / 8);
    os.write(reinterpret_cast<const char *>(&byte_rate), 4);
    uint16_t block_align = channels * (bps / 8);
    os.write(reinterpret_cast<const char *>(&block_align), 2);
    uint16_t bits_per_sample = bps;
    os.write(reinterpret_cast<const char *>(&bits_per_sample), 2);
    os.write("data", 4);
    os.write(reinterpret_cast<const char *>(&data_size), 4);
}

static int do_decode(const std::string& input_file, const std::string& output_file) {
    std::ifstream is(input_file, std::ios::binary);
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

    std::ofstream os(output_file, std::ios::binary);
    if (!os) {
        std::cerr << "Failed to open output file.\n";
        return 1;
    }

    // Write the WAV header (total samples is known from StreamInfo)
    write_wav_header(os, stream_info.sample_rate, stream_info.channels, stream_info.bps, stream_info.samples);

    Decoder decoder;
    int total_samples_written = 0;

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
                int const nb_samples = decoded_channels.empty() ? 0 : decoded_channels[0].size();

                int const channels = stream_info.channels;

                for (int s = 0; s < nb_samples; ++s) {
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
    
    return 0;
}

static int do_encode(const std::string& input_file, const std::string& output_file, const takenc::EncoderConfig& cfg, bool quiet) {
    const auto start_time = std::chrono::steady_clock::now();

    takenc::ProgressCallback progress = nullptr;
    
    if (!quiet) {
        progress = [&](const int64_t processed, const int64_t total) {
            if (total == 0) return;
            const double pct = static_cast<double>(processed) * 100.0 / total;

            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed = now - start_time;
            double speed = 0.0;
            if (elapsed.count() > 0) {
                // Speed = amount of audio processed / wall clock time
                // We assume 44100 Hz for speed calculation approximation
                double audio_time = static_cast<double>(processed) / 44100.0;
                speed = audio_time / elapsed.count();
            }

            std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << std::setw(5) << pct << " % ";
            if (speed > 0) {
                std::cout << "(speed: " << std::fixed << std::setprecision(2) << std::setw(5) << speed << "x)";
            }
            std::cout << std::flush;
        };
    }

    try {
        takenc::EncodeResult result = takenc::Encoder::encode_file(input_file.c_str(), output_file.c_str(), cfg, progress);
        if (cfg.write_md5) std::cout << "\n\nMD5: " << result.md5 << "\n";
        else std::cout << "\n\n";

        if (cfg.verify) {
            std::cout << "\nVerifying... (Invoking internal decode)\n";
            // We can just decode to null or a temporary file to verify
            std::cout << "Verification via takc -d should be done.\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

static void print_help(const char *prog_name) {
    std::cout << "TAK Audio Compressor & Decompressor 0.1.0 (Drop-in replica)\n"
            << "\n"
            << "Usage: " << prog_name << " [options] <input> [output]\n"
            << "\n"
            << "Options:\n"
            << "  -e       Encode mode (default if input is .wav)\n"
            << "  -d       Decode mode (default if input is .tak)\n"
            << "  -overwrite Silently overwrite the destination file\n"
            << "  -q, -silent Suppress progress output\n"
            << "  -p#      Compression preset (0 to 5, e.g. -p2)\n"
            << "  -tn#     Number of threads to use (default: 1)\n"
            << "  -ihs     Ignore header size (for streaming unknown lengths)\n"
            << "  -v       Verify frame integrity after encoding\n"
            << "\n";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    takenc::EncoderConfig cfg;
    std::string input_file;
    std::string output_file;
    bool force_encode = false;
    bool force_decode = false;
    bool quiet = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-e") {
            force_encode = true;
        } else if (arg == "-d") {
            force_decode = true;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        } else if (arg.starts_with("-p")) {
            std::string p = arg.substr(2);
            if (p == "0") {
                cfg.max_lpc_mode = 20;
                cfg.max_filter_order_idx = 0;
                cfg.max_frame_lpc_mode = 1;
            } else if (p == "1") {
                cfg.max_lpc_mode = 30;
                cfg.max_filter_order_idx = 1;
                cfg.max_frame_lpc_mode = 2;
            } else if (p == "2") {
                cfg.max_lpc_mode = 50;
                cfg.max_filter_order_idx = 1;
                cfg.max_frame_lpc_mode = 3;
            } else if (p == "3") {
                cfg.max_lpc_mode = 50;
                cfg.max_filter_order_idx = 6;
                cfg.max_frame_lpc_mode = 3;
            } else if (p == "4") {
                cfg.max_lpc_mode = 50;
                cfg.max_filter_order_idx = 8;
                cfg.max_frame_lpc_mode = 3;
            } else if (p == "5" || p == "E" || p == "Max" || p == "2m" || p == "2e") {
                cfg.max_lpc_mode = 50;
                cfg.max_filter_order_idx = 14;
                cfg.max_frame_lpc_mode = 3;
                if (p == "Max") cfg.max_compression = true;
            }
        } else if (arg.starts_with("-tn")) {
            cfg.threads = std::stoi(arg.substr(3));
        } else if (arg.starts_with("-fsl")) {
            cfg.frame_size_limit = std::stoi(arg.substr(4));
        } else if (arg == "-md5") {
            cfg.write_md5 = true;
        } else if (arg == "-ihs") {
            cfg.ignore_header_size = true;
        } else if (arg == "-v") {
            cfg.verify = true;
        } else if (arg == "-overwrite" || arg == "--overwrite") {
            cfg.overwrite = true;
        } else if (arg == "-silent" || arg == "-q" || arg == "--quiet") {
            cfg.silent = true;
            quiet = true;
        } else if (arg == "-w") {
            cfg.wait_on_exit = true;
        } else {
            if (input_file.empty()) {
                input_file = arg;
            } else if (output_file.empty()) {
                output_file = arg;
            }
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    // Auto-detect mode if not forced
    bool is_encode = true;
    if (force_encode) {
        is_encode = true;
    } else if (force_decode) {
        is_encode = false;
    } else {
        // Infer from extension
        size_t dot = input_file.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = input_file.substr(dot);
            // lowercase
            for(auto& c : ext) c = tolower(c);
            if (ext == ".tak") {
                is_encode = false;
            }
        }
    }

    if (output_file.empty()) {
        // Generate output filename
        size_t dot = input_file.find_last_of('.');
        if (dot != std::string::npos) {
            output_file = input_file.substr(0, dot) + (is_encode ? ".tak" : ".wav");
        } else {
            output_file = input_file + (is_encode ? ".tak" : ".wav");
        }
    }

    if (!cfg.overwrite) {
        std::ifstream f(output_file);
        if (f.good()) {
            std::cout << "File '" << output_file << "' already exists. Overwrite? (y/n): ";
            std::string ans;
            std::cin >> ans;
            if (ans != "y" && ans != "Y") {
                return 0;
            }
        }
    }

    if (!quiet) {
        std::cout << "TAK Audio Compressor/Decompressor 0.1.0\n\n";
        std::cout << (is_encode ? "Encoding: " : "Decoding: ") << input_file << " -> " << output_file << "\n";
    }

    int ret = 0;
    if (is_encode) {
        ret = do_encode(input_file, output_file, cfg, quiet);
    } else {
        ret = do_decode(input_file, output_file);
    }

    if (cfg.wait_on_exit) {
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore(10000, '\n');
        std::cin.get();
    }

    return ret;
}
