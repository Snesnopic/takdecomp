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

struct EncoderConfig {
    int max_lpc_mode = 50;         // max LPC evaluation order (2..50)
    int max_filter_order_idx = 14; // max filter order index to test (0..14)
    bool test_filters = true;      // whether to test predictors at all
    bool max_compression = false;  // whether to perform LPC grid search
    bool test_subframe_splits = true; // whether to do 2-way and 4-way subframe splits
    int max_frame_lpc_mode = 3;    // up to mode 3
    bool write_ape_tag = true;
};

struct EncodeResult {
    std::string md5;
};

using ProgressCallback = std::function<void(int64_t processed, int64_t total)>;

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
    static EncodeResult encode_stream(std::istream& is, std::ostream& os, const EncoderConfig& cfg, ProgressCallback progress = nullptr);
};

} // namespace takenc

#endif // TAK_ENCODER_ENCODER_HPP
