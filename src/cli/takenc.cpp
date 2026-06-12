#include "tak_encoder/encoder.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

void print_help(const char* prog_name) {
    std::cout << "TAK Audio Compressor 0.1.0 (Compatible replica)\n"
              << "\n"
              << "Usage: " << prog_name << " [options] <input.wav> [output.tak]\n"
              << "\n"
              << "Options:\n"
              << "  -e       Encode input.wav to output.tak (default)\n"
              << "  -pM      Preset M (e.g. -p2) - Ignored, defaults to Max\n"
              << "\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    std::string input_file;
    std::string output_file;
    bool encode_mode = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-e") {
            encode_mode = true;
        } else if (arg == "-d") {
            encode_mode = false;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg.starts_with("-p")) {
            // Ignore preset for now
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

    if (output_file.empty()) {
        // Generate output filename
        size_t dot = input_file.find_last_of('.');
        if (dot != std::string::npos) {
            output_file = input_file.substr(0, dot) + ".tak";
        } else {
            output_file = input_file + ".tak";
        }
    }

    if (!encode_mode) {
        std::cerr << "Error: Decoding is handled by takdecomp.\n";
        return 1;
    }

    std::cout << "TAK Audio Compressor 0.1.0\n\n";
    std::cout << "Encoding: " << input_file << " -> " << output_file << "\n";

    auto start_time = std::chrono::steady_clock::now();

    takenc::ProgressCallback progress = [&](int64_t processed, int64_t total) {
        if (total == 0) return;
        double pct = (double)processed * 100.0 / total;
        
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        double speed = 0.0;
        if (elapsed.count() > 0) {
            // Speed = amount of audio processed / wall clock time
            // We assume 44100 Hz for speed calculation approximation
            double audio_time = (double)processed / 44100.0; 
            speed = audio_time / elapsed.count();
        }

        std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << std::setw(5) << pct << " % ";
        if (speed > 0) {
            std::cout << "(speed: " << std::fixed << std::setprecision(2) << std::setw(5) << speed << "x)";
        }
        std::cout << std::flush;
    };

    try {
        takenc::EncodeResult result = takenc::Encoder::encode_file(input_file.c_str(), output_file.c_str(), progress);
        std::cout << "\n\nMD5: " << result.md5 << "\n";
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
