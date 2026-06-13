#include "tak_encoder/encoder.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>

void print_help(const char* prog_name) {
    std::cout << "TAK Audio Compressor 0.1.0 (Compatible replica)\n"
              << "\n"
              << "Usage: " << prog_name << " [options] <input.wav> [output.tak]\n"
              << "\n"
              << "Options:\n"
              << "  -e       Encode input.wav to output.tak (default)\n"
              << "  -pM      Preset M (e.g. -p2) - Ignored, defaults to Max\n"
              << "  -tt #    Add textual tag item # (\"key=value\" or \"key=@file\")\n"
              << "\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    takenc::EncoderConfig cfg;
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
            std::string p = arg.substr(2);
            if (p == "0") {
                cfg.max_lpc_mode = 20; cfg.max_filter_order_idx = 0; cfg.max_frame_lpc_mode = 1;
            } else if (p == "1") {
                cfg.max_lpc_mode = 30; cfg.max_filter_order_idx = 1; cfg.max_frame_lpc_mode = 2;
            } else if (p == "2") {
                cfg.max_lpc_mode = 50; cfg.max_filter_order_idx = 1; cfg.max_frame_lpc_mode = 3;
            } else if (p == "3") {
                cfg.max_lpc_mode = 50; cfg.max_filter_order_idx = 6; cfg.max_frame_lpc_mode = 3;
            } else if (p == "4") {
                cfg.max_lpc_mode = 50; cfg.max_filter_order_idx = 8; cfg.max_frame_lpc_mode = 3;
            } else if (p == "5" || p == "E" || p == "Max" || p == "2m" || p == "2e") {
                cfg.max_lpc_mode = 50; cfg.max_filter_order_idx = 14; cfg.max_frame_lpc_mode = 3;
                if (p == "Max") cfg.max_compression = true;
            }
        } else if (arg == "-tt") {
            if (i + 1 < argc) {
                std::string tag = argv[++i];
                size_t eq = tag.find('=');
                if (eq != std::string::npos) {
                    std::string key = tag.substr(0, eq);
                    std::string val = tag.substr(eq + 1);
                    if (!val.empty() && val[0] == '@') {
                        std::string filename = val.substr(1);
                        std::ifstream fs(filename);
                        if (fs) {
                            std::stringstream buffer;
                            buffer << fs.rdbuf();
                            val = buffer.str();
                        } else {
                            std::cerr << "Warning: could not read tag file '" << filename << "'\n";
                            val = ""; // or keep the @file literal? takc probably fails or ignores. We'll ignore the tag if file doesn't exist.
                            continue;
                        }
                    }
                    cfg.ape_tags[key] = val;
                }
            }
        } else if (arg.starts_with("-tn")) {
            cfg.threads = std::stoi(arg.substr(3));
        } else if (arg.starts_with("-fsl")) {
            cfg.frame_size_limit = std::stoi(arg.substr(4));
        } else if (arg.starts_with("-wm")) {
            cfg.wave_metadata_mode = std::stoi(arg.substr(3));
        } else if (arg == "-md5") {
            cfg.write_md5 = true; // wait, what if default is true? takc enables md5 with -md5, so maybe default is false? I'll keep default true and toggle it if needed, or simply force it true. Actually, let's keep default true since that's what we did, and user can pass -md5. Wait, we should just let -md5 force it to true, but maybe it's already true.
        } else if (arg == "-ihs") {
            cfg.ignore_header_size = true;
        } else if (arg == "-v") {
            cfg.verify = true;
        } else if (arg == "-overwrite") {
            cfg.overwrite = true;
        } else if (arg.starts_with("-fim")) {
            cfg.file_info_mode = std::stoi(arg.substr(4));
        } else if (arg.starts_with("-lf")) {
            cfg.log_file_format = arg.substr(3);
        } else if (arg.starts_with("-l")) {
            // Check if it's -lp or -l#
            if (arg == "-lp") {
                cfg.low_priority = true;
            } else {
                // assume -l#
                cfg.log_level = std::stoi(arg.substr(2));
            }
        } else if (arg == "-silent") {
            cfg.silent = true;
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
        std::cerr << "Error: Decoding is handled by takdec.\n";
        return 1;
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
        takenc::EncodeResult result = takenc::Encoder::encode_file(input_file.c_str(), output_file.c_str(), cfg, progress);
        if (cfg.write_md5) std::cout << "\n\nMD5: " << result.md5 << "\n";
        else std::cout << "\n\n";

        if (cfg.verify) {
            std::cout << "\nVerifying... (Invoking takdec)\n";
            std::string exe_path = argv[0];
            size_t slash = exe_path.find_last_of("/\\");
            std::string dir = (slash != std::string::npos) ? exe_path.substr(0, slash + 1) : "./";
            std::string cmd = dir + "takdec " + output_file + " -t";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "Verification failed!\n";
                return 1;
            }
            std::cout << "Verification successful.\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }

    if (cfg.wait_on_exit) {
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore(10000, '\n');
        std::cin.get();
    }

    return 0;
}
