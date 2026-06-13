#ifndef TAK_ENCODER_ENCODER_HPP
#define TAK_ENCODER_ENCODER_HPP

#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <cstdint>
#include <vector>
#include <span>
#include "tak_encoder/bitstream_writer.hpp"
#include <map>
#include <functional>

namespace takenc {

struct CParam {
    int8_t init;
    uint32_t escape;
    uint32_t scale;
    uint32_t aescape;
    uint32_t bias;
};

extern const CParam xcodes[50];

/**
 * @brief Configuration object for the TAK encoder.
 * 
 * Contains all the parameters affecting compression ratio, processing speed, 
 * multithreading, and CLI parity behaviors.
 */
struct EncoderConfig {
    int max_lpc_mode = 50;         // max LPC evaluation order (2..50)
    int max_filter_order_idx = 14; // max filter order index to test (0..14)
    bool test_filters = true;      // whether to test predictors at all
    bool max_compression = false;  // whether to perform LPC grid search
    bool test_subframe_splits = true; // whether to do 2-way and 4-way subframe splits
    int max_frame_lpc_mode = 3;    // up to mode 3
    bool write_ape_tag = true;
    std::map<std::string, std::string> ape_tags;

    // CLI parity options
    int threads = 1;
    int frame_size_limit = 0; // 0 means default
    int wave_metadata_mode = 1; // 0=disable, 1=default max size, 46..1048576=custom max size
    bool write_md5 = true;
    bool ignore_header_size = false;
    bool verify = false;
    bool overwrite = false;
    int file_info_mode = 0;
    int log_level = 0;
    std::string log_file_format = "Ansi";
    bool silent = false;
    bool wait_on_exit = false;
    bool low_priority = false;
};

/**
 * @brief Result object returned by the encoding functions.
 */
struct EncodeResult {
    std::string md5;
};

using ProgressCallback = std::function<void(int64_t processed, int64_t total)>;

/**
 * @brief Main encoder class for TAK audio files.
 * 
 * This class handles the compression of PCM audio samples into TAK bitstreams.
 * It provides both stream-based and file-based API endpoints.
 */
class Encoder {
public:
    Encoder() = default;

    static void encode_segment(int mode, const int32_t* data, int len, BitStreamWriter& bw);
    static int calc_bits_needed(int mode, const int32_t* data, int len);

    struct ResiduesPartition {
        int cost;
        int n;
        std::vector<int> vs;
        std::vector<int> modes;
    };
    static ResiduesPartition plan_residues_partition(const int32_t* data, int length);

    static EncodeResult encode_file(const char* wav_path, const char* tak_path, const EncoderConfig& cfg, ProgressCallback progress = nullptr);
    /**
     * @brief Encodes an input stream containing WAV PCM data into a TAK output stream.
     * 
     * @param is The input stream containing the raw WAV file.
     * @param os The output stream where the TAK bitstream will be written.
     * @param cfg The encoder configuration parameters.
     * @param progress Optional callback function for reporting progress.
     * @return EncodeResult Struct containing the final MD5 hash.
     */
    static EncodeResult encode_stream(std::istream& is, std::ostream& os, const EncoderConfig& cfg, ProgressCallback progress = nullptr);
};

} // namespace takenc

#endif // TAK_ENCODER_ENCODER_HPP
