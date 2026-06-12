#include <cstdint>
#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <stdexcept>
#include <array>
#include <utility>

namespace takdecomp {

namespace {

struct CParam {
    int8_t init;
    uint32_t escape;
    uint32_t scale;
    uint32_t aescape;
    uint32_t bias;
} __attribute__((packed)) __attribute__((aligned(32)));

constexpr std::array<CParam, 50> xcodes = {{
    { .init=0x01, .escape=0x0000001, .scale=0x0000001, .aescape=0x0000003, .bias=0x0000008 },
    { .init=0x02, .escape=0x0000003, .scale=0x0000001, .aescape=0x0000007, .bias=0x0000006 },
    { .init=0x03, .escape=0x0000005, .scale=0x0000002, .aescape=0x000000E, .bias=0x000000D },
    { .init=0x03, .escape=0x0000003, .scale=0x0000003, .aescape=0x000000D, .bias=0x0000018 },
    { .init=0x04, .escape=0x000000B, .scale=0x0000004, .aescape=0x000001C, .bias=0x0000019 },
    { .init=0x04, .escape=0x0000006, .scale=0x0000006, .aescape=0x000001A, .bias=0x0000030 },
    { .init=0x05, .escape=0x0000016, .scale=0x0000008, .aescape=0x0000038, .bias=0x0000032 },
    { .init=0x05, .escape=0x000000C, .scale=0x000000C, .aescape=0x0000034, .bias=0x0000060 },
    { .init=0x06, .escape=0x000002C, .scale=0x0000010, .aescape=0x0000070, .bias=0x0000064 },
    { .init=0x06, .escape=0x0000018, .scale=0x0000018, .aescape=0x0000068, .bias=0x00000C0 },
    { .init=0x07, .escape=0x0000058, .scale=0x0000020, .aescape=0x00000E0, .bias=0x00000C8 },
    { .init=0x07, .escape=0x0000030, .scale=0x0000030, .aescape=0x00000D0, .bias=0x0000180 },
    { .init=0x08, .escape=0x00000B0, .scale=0x0000040, .aescape=0x00001C0, .bias=0x0000190 },
    { .init=0x08, .escape=0x0000060, .scale=0x0000060, .aescape=0x00001A0, .bias=0x0000300 },
    { .init=0x09, .escape=0x0000160, .scale=0x0000080, .aescape=0x0000380, .bias=0x0000320 },
    { .init=0x09, .escape=0x00000C0, .scale=0x00000C0, .aescape=0x0000340, .bias=0x0000600 },
    { .init=0x0A, .escape=0x00002C0, .scale=0x0000100, .aescape=0x0000700, .bias=0x0000640 },
    { .init=0x0A, .escape=0x0000180, .scale=0x0000180, .aescape=0x0000680, .bias=0x0000C00 },
    { .init=0x0B, .escape=0x0000580, .scale=0x0000200, .aescape=0x0000E00, .bias=0x0000C80 },
    { .init=0x0B, .escape=0x0000300, .scale=0x0000300, .aescape=0x0000D00, .bias=0x0001800 },
    { .init=0x0C, .escape=0x0000B00, .scale=0x0000400, .aescape=0x0001C00, .bias=0x0001900 },
    { .init=0x0C, .escape=0x0000600, .scale=0x0000600, .aescape=0x0001A00, .bias=0x0003000 },
    { .init=0x0D, .escape=0x0001600, .scale=0x0000800, .aescape=0x0003800, .bias=0x0003200 },
    { .init=0x0D, .escape=0x0000C00, .scale=0x0000C00, .aescape=0x0003400, .bias=0x0006000 },
    { .init=0x0E, .escape=0x0002C00, .scale=0x0001000, .aescape=0x0007000, .bias=0x0006400 },
    { .init=0x0E, .escape=0x0001800, .scale=0x0001800, .aescape=0x0006800, .bias=0x000C000 },
    { .init=0x0F, .escape=0x0005800, .scale=0x0002000, .aescape=0x000E000, .bias=0x000C800 },
    { .init=0x0F, .escape=0x0003000, .scale=0x0003000, .aescape=0x000D000, .bias=0x0018000 },
    { .init=0x10, .escape=0x000B000, .scale=0x0004000, .aescape=0x001C000, .bias=0x0019000 },
    { .init=0x10, .escape=0x0006000, .scale=0x0006000, .aescape=0x001A000, .bias=0x0030000 },
    { .init=0x11, .escape=0x0016000, .scale=0x0008000, .aescape=0x0038000, .bias=0x0032000 },
    { .init=0x11, .escape=0x000C000, .scale=0x000C000, .aescape=0x0034000, .bias=0x0060000 },
    { .init=0x12, .escape=0x002C000, .scale=0x0010000, .aescape=0x0070000, .bias=0x0064000 },
    { .init=0x12, .escape=0x0018000, .scale=0x0018000, .aescape=0x0068000, .bias=0x00C0000 },
    { .init=0x13, .escape=0x0058000, .scale=0x0020000, .aescape=0x00E0000, .bias=0x00C8000 },
    { .init=0x13, .escape=0x0030000, .scale=0x0030000, .aescape=0x00D0000, .bias=0x0180000 },
    { .init=0x14, .escape=0x00B0000, .scale=0x0040000, .aescape=0x01C0000, .bias=0x0190000 },
    { .init=0x14, .escape=0x0060000, .scale=0x0060000, .aescape=0x01A0000, .bias=0x0300000 },
    { .init=0x15, .escape=0x0160000, .scale=0x0080000, .aescape=0x0380000, .bias=0x0320000 },
    { .init=0x15, .escape=0x00C0000, .scale=0x00C0000, .aescape=0x0340000, .bias=0x0600000 },
    { .init=0x16, .escape=0x02C0000, .scale=0x0100000, .aescape=0x0700000, .bias=0x0640000 },
    { .init=0x16, .escape=0x0180000, .scale=0x0180000, .aescape=0x0680000, .bias=0x0C00000 },
    { .init=0x17, .escape=0x0580000, .scale=0x0200000, .aescape=0x0E00000, .bias=0x0C80000 },
    { .init=0x17, .escape=0x0300000, .scale=0x0300000, .aescape=0x0D00000, .bias=0x1800000 },
    { .init=0x18, .escape=0x0B00000, .scale=0x0400000, .aescape=0x1C00000, .bias=0x1900000 },
    { .init=0x18, .escape=0x0600000, .scale=0x0600000, .aescape=0x1A00000, .bias=0x3000000 },
    { .init=0x19, .escape=0x1600000, .scale=0x0800000, .aescape=0x3800000, .bias=0x3200000 },
    { .init=0x19, .escape=0x0C00000, .scale=0x0C00000, .aescape=0x3400000, .bias=0x6000000 },
    { .init=0x1A, .escape=0x2C00000, .scale=0x1000000, .aescape=0x7000000, .bias=0x6400000 },
    { .init=0x1A, .escape=0x1800000, .scale=0x1800000, .aescape=0x6800000, .bias=0xC000000 },
}};

} // namespace

auto Decoder::get_unary(BitStreamReader& gb, int stop, int len) -> int {
    int i;
    for (i = 0; i < len && std::cmp_not_equal(gb.get_bits1() , stop); i++) {
        ;
}
    return i;
}

void Decoder::decode_segment(int8_t mode, int32_t* decoded, int len, BitStreamReader& gb) {
    if (mode == 0) {
        for (int i = 0; i < len; ++i) {
            decoded[i] = 0;
        }
        return;
    }
    if (mode < 0 || mode > static_cast<int8_t>(xcodes.size())) {
        throw std::runtime_error("Invalid decode_segment mode");
    }
    const auto& code = xcodes[mode - 1];

    for (int i = 0; i < len; i++) {
        if (mode == 28) {
            
        }
        int32_t x = gb.get_bits(code.init);
        if (std::cmp_greater_equal(x, code.escape) && (gb.get_bits1() != 0u)) {
            x |= 1 << code.init;
            if (std::cmp_greater_equal(x, code.aescape)) {
                int32_t scale = get_unary(gb, 1, 9);
                if (scale == 9) {
                    int scale_bits = gb.get_bits(3);
                    if (scale_bits > 0) {
                        if (scale_bits == 7) {
                            scale_bits += gb.get_bits(5);
                            if (scale_bits > 29) {
                                
                                
                                
                                throw std::runtime_error("Invalid scale bits");
                            }
                        }
                        scale = gb.get_bits(scale_bits) + 1;
                        x += code.scale * scale;
                    }
                    x += code.bias;
                } else {
                    x += (code.scale * scale) - code.escape;
                }
            } else {
                x -= code.escape;
            }
        }
        decoded[i] = (x >> 1) ^ -(x & 1);
    }
}

void Decoder::decode_lpc(int32_t* coeffs, int mode, int length) {
    if (length < 2) { return;
}

    if (mode == 1) {
        uint32_t a1 = *coeffs++;
        for (int i = 0; i < (length - 1) >> 1; i++) {
            *coeffs += a1;
            coeffs[1] += static_cast<uint32_t>(*coeffs);
            a1 = coeffs[1];
            coeffs += 2;
        }
        if (((length - 1) & 1) != 0) {
            *coeffs += a1;
        }
    } else if (mode == 2) {
        uint32_t a1 = coeffs[1];
        uint32_t a2 = a1 + *coeffs;
        coeffs[1] = a2;
        if (length > 2) {
            coeffs += 2;
            for (int i = 0; i < (length - 2) >> 1; i++) {
                uint32_t const a3 = *coeffs + a1;
                uint32_t const a4 = a3 + a2;
                *coeffs = a4;
                a1 = coeffs[1] + a3;
                a2 = a1 + a4;
                coeffs[1] = a2;
                coeffs += 2;
            }
            if ((length & 1) != 0) {
                *coeffs += a1 + a2;
            }
        }
    } else if (mode == 3) {
        uint32_t const a1 = coeffs[1];
        uint32_t const a2 = a1 + *coeffs;
        coeffs[1] = a2;
        if (length > 2) {
            uint32_t a3 = coeffs[2];
            uint32_t a4 = a3 + a1;
            uint32_t a5 = a4 + a2;
            coeffs[2] = a5;
            coeffs += 3;
            for (int i = 0; i < length - 3; i++) {
                a3 += *coeffs;
                a4 += a3;
                a5 += a4;
                *coeffs = a5;
                coeffs++;
            }
        }
    }
}

void Decoder::decode_residues(int32_t* decoded, int length, BitStreamReader& gb) {
    if (length > nb_samples_) {
        throw std::runtime_error("Residue length > nb_samples");
    }

    if (gb.get_bits1() != 0u) {
        int wlength = length / uval_;
        int rval = length - (wlength * uval_);

        if (rval < uval_ / 2) {
            rval += uval_;
        } else {
            wlength++;
        }

        if (wlength <= 1 || wlength > 128) {
            throw std::runtime_error("Invalid wlength");
        }

        int mode = gb.get_bits(6);
        coding_mode_[0] = mode;

        for (int i = 1; i < wlength; i++) {
            int const c = get_unary(gb, 1, 6);

            switch (c) {
            case 6:
                mode = gb.get_bits(6);
                break;
            case 5:
            case 4:
            case 3: {
                int const sign = gb.get_bits1();
                mode += (-sign ^ (c - 1)) + sign;
                break;
            }
            case 2:
                mode++;
                break;
            case 1:
                mode--;
                break;
            }
            coding_mode_[i] = mode;
        }

        int i = 0;
        while (i < wlength) {
            int len = 0;
            mode = coding_mode_[i];
            do {
                if (i >= wlength - 1) {
                    len += rval;
                } else {
                    len += uval_;
                }
                i++;
                if (i == wlength) { break;
}
            } while (coding_mode_[i] == mode);

            decode_segment(mode, decoded, len, gb);
            decoded += len;
        }
    } else {
        int const mode = gb.get_bits(6);
        decode_segment(mode, decoded, length, gb);
    }
}

} // namespace takdecomp
