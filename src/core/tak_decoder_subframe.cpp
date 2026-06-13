#include <iostream>
#include <cstdint>
#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <array>

namespace takdecomp {
    namespace {
        constexpr std::array<uint16_t, 16> predictor_sizes = {
            4, 8, 12, 16, 24, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256, 0,
        };


        int32_t clip_intp2(int32_t a, int p) {
            if (((static_cast<unsigned>(a) + (1 << p)) & ~((2U << p) - 1)) != 0u) {
                return (a < 0 ? -1 : 0) ^ ((1 << p) - 1);
            }
            return a;
        }
    } // namespace

    void Decoder::decode_subframe(int32_t *decoded, int subframe_size, int prev_subframe_size, BitStreamReader &gb) {
        if (gb.get_bits1() == 0u) {
            decode_residues(decoded, subframe_size, gb);
            return;
        }

        int const filter_order = predictor_sizes[gb.get_bits(4)];

        if (prev_subframe_size > 0 && (gb.get_bits1() != 0u)) {
            if (filter_order > prev_subframe_size) {
                throw std::runtime_error("Filter order > prev_subframe_size");
            }

            decoded -= filter_order;
            subframe_size += filter_order;

            if (filter_order > subframe_size) {
                throw std::runtime_error("Filter order > subframe_size");
            }
        } else {
            if (filter_order > subframe_size) {
                throw std::runtime_error("Filter order > subframe_size");
            }

            int const lpc_mode = gb.get_bits(2);
            if (lpc_mode > 2) {
                throw std::runtime_error("Invalid lpc_mode");
            }

            decode_residues(decoded, filter_order, gb);

            if (lpc_mode != 0) {
                decode_lpc(decoded, lpc_mode, filter_order);
            }
        }

        int const dshift = (gb.get_bits1() != 0u) ? gb.get_bits(4) + 1 : 0;
        int const size = gb.get_bits1() + 6;

        int filter_quant = 10;
        if (gb.get_bits1() != 0u) {
            filter_quant -= gb.get_bits(3) + 1;
            if (filter_quant < 3) {
                throw std::runtime_error("Invalid filter_quant");
            }
        }


        if (gb.get_bits_left() < static_cast<size_t>((2 * 10) + (2 * size))) {
            throw std::runtime_error("Not enough bits");
        }

        predictors_[0] = gb.get_sbits(10);
        predictors_[1] = gb.get_sbits(10);
        predictors_[2] = gb.get_sbits(size) * (1 << (10 - size));
        predictors_[3] = gb.get_sbits(size) * (1 << (10 - size));

        if (filter_order > 4) {
            int const tmp = size - gb.get_bits1();
            int x = tmp;
            for (int i = 4; i < filter_order; i++) {
                if ((i & 3) == 0) {
                    x = tmp - gb.get_bits(2);
                }
                predictors_[i] = gb.get_sbits(x) * (1 << (10 - size));
            }
        }

        std::array<int, 256> tfilter{};
        tfilter[0] = predictors_[0] * 64;
        for (int i = 1; i < filter_order; i++) {
            for (int j = 0; j < (i + 1) / 2; j++) {
                int32_t const v1 = tfilter[j];
                int32_t const v2 = tfilter[i - 1 - j];
                int32_t const x = v1 + (((static_cast<int32_t>(predictors_[i]) * v2) + 256) >> 9);
                tfilter[i - 1 - j] = v2 + (((static_cast<int32_t>(predictors_[i]) * v1) + 256) >> 9);
                tfilter[j] = x;
            }

            tfilter[i] = predictors_[i] * 64;
        }

        int x = 1 << (32 - (15 - filter_quant));
        int y = 1 << ((15 - filter_quant) - 1);
        for (int i = 0, j = filter_order - 1; i < filter_order / 2; i++, j--) {
            filter_[j] = x - ((tfilter[i] + y) >> (15 - filter_quant));
            filter_[i] = x - ((tfilter[j] + y) >> (15 - filter_quant));
        }

        decode_residues(&decoded[filter_order], subframe_size - filter_order, gb);

        for (int i = 0; i < filter_order; i++) {
            residues_[i] = decoded[i] >> dshift;
        }

        y = residues_.size() - filter_order;
        x = subframe_size - filter_order;

        int32_t *dec_ptr = decoded + filter_order;

        while (x > 0) {
            int const tmp = std::min(y, x);

            for (int i = 0; i < tmp; i++) {
                int v = 1 << (filter_quant - 1);

                for (int j = 0; j < filter_order; j++) {
                    v += residues_[i + j] * static_cast<unsigned>(filter_[j]);
                }
                v = (clip_intp2(v >> filter_quant, 13) * (1 << dshift)) - static_cast<unsigned>(*dec_ptr);
                *dec_ptr++ = v;
                residues_[filter_order + i] = v >> dshift;
            }

            x -= tmp;
            if (x > 0) {
                std::memmove(residues_.data(), residues_.data() + y, 2 * filter_order);
            }
        }
    }

    void Decoder::decode_channel(int chan, BitStreamReader &gb) {
        int32_t *decoded = decoded_[chan].data();
        int left = nb_samples_ - 1;
        int prev = 0;

        sample_shift_[chan] = (gb.get_bits1() != 0u) ? gb.get_bits(4) + 1 : 0;
        if (sample_shift_[chan] >= bps_) {
            throw std::runtime_error("Invalid sample shift");
        }

        *decoded++ = gb.get_sbits(bps_ - sample_shift_[chan]);
        lpc_mode_[chan] = gb.get_bits(2);

        nb_subframes_ = gb.get_bits(3) + 1;


        int i = 0;
        if (nb_subframes_ > 1) {
            if (gb.get_bits_left() < static_cast<size_t>((nb_subframes_ - 1) * 6)) {
                throw std::runtime_error("Not enough bits");
            }

            for (; i < nb_subframes_ - 1; i++) {
                int const v = gb.get_bits(6);

                subframe_len_[i] = (v - prev) * subframe_scale_;
                if (subframe_len_[i] <= 0) {
                    throw std::runtime_error("Invalid subframe_len");
                }

                left -= subframe_len_[i];
                prev = v;
            }

            if (left <= 0) {
                throw std::runtime_error("Invalid left samples");
            }
        }
        subframe_len_[i] = left;

        prev = 0;
        for (i = 0; i < nb_subframes_; i++) {
            decode_subframe(decoded, subframe_len_[i], prev, gb);
            decoded += subframe_len_[i];

            prev = subframe_len_[i];
        }
    }
} // namespace takdecomp
