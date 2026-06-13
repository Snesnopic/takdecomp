#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <algorithm>
#include <cstdint>
#include <array>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace takdecomp {
    void Decoder::decorrelate(const int c1,const int c2, int length, BitStreamReader &gb) {
        int32_t *p1 = decoded_[c1].data() + (dmode_ > 5 ? 1 : 0);
        int32_t *p2 = decoded_[c2].data() + (dmode_ > 5 ? 1 : 0);

        int32_t bp1 = p1[0];
        int32_t bp2 = p2[0];
        int dshift = 0;
        int dfactor = 0;


        length += (dmode_ < 6 ? 1 : 0);

        switch (dmode_) {
            case 1: /* left/side: p2 = p1 + p2 */
                for (int i = 0; i < length; i++) {
                    uint32_t const a = p1[i];
                    uint32_t const b = p2[i];
                    p2[i] = a + b;
                }
                break;
            case 2: /* side/right: p1 = p2 - p1 */
                for (int i = 0; i < length; i++) {
                    uint32_t const a = p1[i];
                    uint32_t const b = p2[i];
                    p1[i] = b - a;
                }
                break;
            case 3: /* side/mid */
                for (int i = 0; i < length; i++) {
                    uint32_t a = p1[i];
                    int32_t const b = p2[i];
                    a -= b >> 1; // arithmetic right shift, not division
                    p1[i] = a;
                    p2[i] = a + b;
                }
                break;
            case 4: /* side/left with scale factor */
                std::swap(p1, p2);
                std::swap(bp1, bp2);
                [[fallthrough]];
            case 5: /* side/right with scale factor */
                dshift = (gb.get_bits1() != 0u) ? gb.get_bits(4) + 1 : 0;
                dfactor = gb.get_sbits(10);
                for (int i = 0; i < length; i++) {
                    uint32_t const a = p1[i];
                    int32_t b = p2[i];
                    b = static_cast<unsigned>(static_cast<int>((dfactor * static_cast<unsigned>(b >> dshift)) + 128) >>
                                              8) << dshift;
                    p1[i] = b - a;
                }
                break;
            case 6:
                std::swap(p1, p2);
                [[fallthrough]];
            case 7: {
                if (length < 256) {
                    throw std::runtime_error("Invalid decorrelate length");
                }

                dshift = (gb.get_bits1() != 0u) ? gb.get_bits(4) + 1 : 0;

                int const filter_order = 8 << gb.get_bits1();
                int const dval1 = gb.get_bits1();
                int const dval2 = gb.get_bits1();


                int code_size = 0;
                for (int i = 0; i < filter_order; i++) {
                    if ((i & 3) == 0) {
                        code_size = 14 - gb.get_bits(3);
                    }
                    filter_[i] = gb.get_sbits(code_size);
                }

                int const order_half = filter_order / 2;
                int length2 = length - (filter_order - 1);


                /* decorrelate beginning samples */
                if (dval1 != 0) {
                    for (int i = 0; i < order_half; i++) {
                        int32_t const a = p1[i];
                        int32_t const b = p2[i];
                        p1[i] = a + b;
                    }
                }

                for (int i = 0; i < filter_order; i++) {
                    residues_[i] = *p2++ >> dshift;
                }

                p1 += order_half;
                int const x = residues_.size() - filter_order;
                int tmp;
                for (; length2 > 0; length2 -= tmp) {
                    tmp = std::min(length2, x);

                    for (int i = 0; i < tmp - (tmp == length2 ? 1 : 0); i++) {
                        residues_[filter_order + i] = *p2++ >> dshift;
                    }

                    for (int i = 0; i < tmp; i++) {
                        int v = 1 << 9;

                        if (filter_order == 16) {
                            for (int j = 0; j < 16; j++) {
                                v += residues_[i + j] * filter_[j];
                            }
                        } else {
                            v += (residues_[i + 7] * filter_[7]) +
                                    (residues_[i + 6] * filter_[6]) +
                                    (residues_[i + 5] * filter_[5]) +
                                    (residues_[i + 4] * filter_[4]) +
                                    (residues_[i + 3] * filter_[3]) +
                                    (residues_[i + 2] * filter_[2]) +
                                    (residues_[i + 1] * filter_[1]) +
                                    (residues_[i] * filter_[0]);
                        }

                        auto clip_intp2 = [](const int32_t a, const int p) -> int32_t {
                            if ((static_cast<unsigned>(a) + (1 << p)) & ~((2U << p) - 1)) {
                                return (a < 0 ? -1 : 0) ^ ((1 << p) - 1);
                            }
                            return a;
                        };

                        v = (clip_intp2(v >> 10, 13) * (1U << dshift)) - *p1;
                        *p1++ = v;
                    }

                    memmove(residues_.data(), &residues_[tmp], 2 * filter_order);
                }

                p1 = decoded_[c1].data() + (dmode_ > 5 ? 1 : 0);
                p2 = decoded_[c2].data() + (dmode_ > 5 ? 1 : 0);
                if (dmode_ == 6) std::swap(p1, p2);

                if (dval2 != 0) {
                    for (int i = length - order_half; i < length; i++) {
                        int32_t const a = p1[i];
                        int32_t const b = p2[i];
                        p1[i] = a + b;
                    }
                }
                break;
            }
        }

        if (dmode_ > 0 && dmode_ < 6) {
            p1[0] = bp1;
            p2[0] = bp2;
        }
    }
} // namespace takdecomp
