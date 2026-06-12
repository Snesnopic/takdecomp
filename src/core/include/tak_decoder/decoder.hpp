#ifndef TAK_DECODER_DECODER_HPP
#define TAK_DECODER_DECODER_HPP

#include "streaminfo.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace takdecomp {

class BitStreamReader;

class Decoder {
public:
    Decoder() = default;

    // Parse the stream info block from the bitstream.
    // Throws std::runtime_error on invalid data.
    auto parse_streaminfo(BitStreamReader& gb) -> StreamInfo;


    // Decode a frame header
    void decode_frame_header(BitStreamReader& gb, StreamInfo& info);

    // Decodes the current frame from the bitstream. Returns consumed bytes.
    auto decode_frame(std::span<const uint8_t> data, StreamInfo& info, std::vector<std::vector<int32_t>>& output) -> size_t;

private:
    static void decode_lpc(int32_t* coeffs, int mode, int length);
    void decode_segment(int8_t mode, int32_t* decoded, int len, BitStreamReader& gb);
    void decode_residues(int32_t* decoded, int length, BitStreamReader& gb);
    static auto get_unary(BitStreamReader& gb, int step, int max) -> int;
    
    void decode_subframe(int32_t* decoded, int subframe_size, int prev_subframe_size, BitStreamReader& gb);
    void decode_channel(int chan, BitStreamReader& gb);
    void decorrelate(int c1, int c2, int length, BitStreamReader& gb);

    static auto get_nb_samples(int sample_rate, FrameSizeType type) -> int;
    static auto check_crc24(std::span<const uint8_t> data) -> bool;

    int uval_ = 0;
    int subframe_scale_ = 0;
    int nb_samples_ = 0;
    int bps_ = 0;
    int channels_ = 0;
    int sample_rate_ = 0;

    std::vector<std::vector<int32_t>> decoded_;
    
    std::array<int8_t, constants::MAX_CHANNELS> lpc_mode_{};
    std::array<int8_t, constants::MAX_CHANNELS> sample_shift_{};
    std::array<int16_t, 256> predictors_{};
    int nb_subframes_ = 0;
    std::array<int16_t, 8> subframe_len_{};
    
    struct MCDParam {
        int8_t present = 0;
        int8_t index = 0;
        int8_t chan1 = 0;
        int8_t chan2 = 0;
    } __attribute__((aligned(4)));
    std::array<MCDParam, constants::MAX_CHANNELS> mcdparams_{};

    int8_t dmode_ = 0;
    std::array<int8_t, 128> coding_mode_{};
    std::array<int16_t, 256> filter_{};
    std::array<int16_t, 544> residues_{};
};

} // namespace takdecomp
#endif // TAK_DECODER_DECODER_HPP
