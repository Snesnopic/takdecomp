#ifndef TAK_DECODER_DECODER_HPP
#define TAK_DECODER_DECODER_HPP

#include "streaminfo.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace takdecomp {
    class BitStreamReader;

    /**
     * @brief Main decoder class for TAK audio files.
     *
     * This class handles the parsing and decoding of TAK bitstreams,
     * expanding the compressed frames back into raw PCM audio samples.
     */
    class Decoder {
    public:
        Decoder() = default;

        /**
         * @brief Parses the stream info block from the bitstream.
         *
         * @param gb The BitStreamReader initialized with the stream info data.
         * @return StreamInfo The parsed stream metadata.
         * @throws std::runtime_error if the data is invalid.
         */
        static StreamInfo parse_streaminfo(BitStreamReader &gb);


        /**
         * @brief Decodes the header of an audio frame.
         *
         * @param gb The BitStreamReader pointing to the start of the frame.
         * @param info Stream information to update or use during parsing.
         */
        static void decode_frame_header(BitStreamReader &gb, StreamInfo &info);

        /**
         * @brief Decodes the current audio frame from the bitstream into PCM samples.
         *
         * @param data The raw bitstream data of the frame.
         * @param info Stream information context.
         * @param output The multi-channel output buffer to store decoded samples.
         * @return size_t The number of bytes consumed from the data span.
         */
        size_t decode_frame(std::span<const uint8_t> data, StreamInfo &info,
                            std::vector<std::vector<int32_t> > &output);

    private:
        static void decode_lpc(int32_t *coeffs, int mode, int length);

        static void decode_segment(int8_t mode, int32_t *decoded, int len, BitStreamReader &gb);

        void decode_residues(int32_t *decoded, int length, BitStreamReader &gb);

        static int get_unary(BitStreamReader &gb, int step, int max);

        void decode_subframe(int32_t *decoded, int subframe_size, int prev_subframe_size, BitStreamReader &gb);

        void decode_channel(int chan, BitStreamReader &gb);

        void decorrelate(int c1, int c2, int length, BitStreamReader &gb);

        static int get_nb_samples(int sample_rate, FrameSizeType type);

        static bool check_crc24(std::span<const uint8_t> data);

        int uval_ = 0;
        int subframe_scale_ = 0;
        int nb_samples_ = 0;
        int bps_ = 0;
        int channels_ = 0;
        int sample_rate_ = 0;

        std::vector<std::vector<int32_t> > decoded_;

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
        };

        std::array<MCDParam, constants::MAX_CHANNELS> mcdparams_{};

        int8_t dmode_ = 0;
        std::array<int8_t, 128> coding_mode_{};
        std::array<int16_t, 256> filter_{};
        std::array<int16_t, 544> residues_{};
    };
} // namespace takdecomp
#endif // TAK_DECODER_DECODER_HPP
