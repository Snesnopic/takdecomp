#include <iostream>
#include "tak_decoder/decoder.hpp"
#include "tak_decoder/bitstream.hpp"
#include <stdexcept>
#include <array>

namespace takdecomp {

namespace {

struct CParam {
    int8_t init;
    uint32_t escape;
    uint32_t scale;
    uint32_t aescape;
    uint32_t bias;
};

constexpr std::array<CParam, 50> xcodes = {{
    { 0x01, 0x0000001, 0x0000001, 0x0000003, 0x0000008 },
    { 0x02, 0x0000003, 0x0000001, 0x0000007, 0x0000006 },
    { 0x03, 0x0000005, 0x0000002, 0x000000E, 0x000000D },
    { 0x03, 0x0000003, 0x0000003, 0x000000D, 0x0000018 },
    { 0x04, 0x000000B, 0x0000004, 0x000001C, 0x0000019 },
    { 0x04, 0x0000006, 0x0000006, 0x000001A, 0x0000030 },
    { 0x05, 0x0000016, 0x0000008, 0x0000038, 0x0000032 },
    { 0x05, 0x000000C, 0x000000C, 0x0000034, 0x0000060 },
    { 0x06, 0x000002C, 0x0000010, 0x0000070, 0x0000064 },
    { 0x06, 0x0000018, 0x0000018, 0x0000068, 0x00000C0 },
    { 0x07, 0x0000058, 0x0000020, 0x00000E0, 0x00000C8 },
    { 0x07, 0x0000030, 0x0000030, 0x00000D0, 0x0000180 },
    { 0x08, 0x00000B0, 0x0000040, 0x00001C0, 0x0000190 },
    { 0x08, 0x0000060, 0x0000060, 0x00001A0, 0x0000300 },
    { 0x09, 0x0000160, 0x0000080, 0x0000380, 0x0000320 },
    { 0x09, 0x00000C0, 0x00000C0, 0x0000340, 0x0000600 },
    { 0x0A, 0x00002C0, 0x0000100, 0x0000700, 0x0000640 },
    { 0x0A, 0x0000180, 0x0000180, 0x0000680, 0x0000C00 },
    { 0x0B, 0x0000580, 0x0000200, 0x0000E00, 0x0000C80 },
    { 0x0B, 0x0000300, 0x0000300, 0x0000D00, 0x0001800 },
    { 0x0C, 0x0000B00, 0x0000400, 0x0001C00, 0x0001900 },
    { 0x0C, 0x0000600, 0x0000600, 0x0001A00, 0x0003000 },
    { 0x0D, 0x0001600, 0x0000800, 0x0003800, 0x0003200 },
    { 0x0D, 0x0000C00, 0x0000C00, 0x0003400, 0x0006000 },
    { 0x0E, 0x0002C00, 0x0001000, 0x0007000, 0x0006400 },
    { 0x0E, 0x0001800, 0x0001800, 0x0006800, 0x000C000 },
    { 0x0F, 0x0005800, 0x0002000, 0x000E000, 0x000C800 },
    { 0x0F, 0x0003000, 0x0003000, 0x000D000, 0x0018000 },
    { 0x10, 0x000B000, 0x0004000, 0x001C000, 0x0019000 },
    { 0x10, 0x0006000, 0x0006000, 0x001A000, 0x0030000 },
    { 0x11, 0x0016000, 0x0008000, 0x0038000, 0x0032000 },
    { 0x11, 0x000C000, 0x000C000, 0x0034000, 0x0060000 },
    { 0x12, 0x002C000, 0x0010000, 0x0070000, 0x0064000 },
    { 0x12, 0x0018000, 0x0018000, 0x0068000, 0x00C0000 },
    { 0x13, 0x0058000, 0x0020000, 0x00E0000, 0x00C8000 },
    { 0x13, 0x0030000, 0x0030000, 0x00D0000, 0x0180000 },
    { 0x14, 0x00B0000, 0x0040000, 0x01C0000, 0x0190000 },
    { 0x14, 0x0060000, 0x0060000, 0x01A0000, 0x0300000 },
    { 0x15, 0x0160000, 0x0080000, 0x0380000, 0x0320000 },
    { 0x15, 0x00C0000, 0x00C0000, 0x0340000, 0x0600000 },
    { 0x16, 0x02C0000, 0x0100000, 0x0700000, 0x0640000 },
    { 0x16, 0x0180000, 0x0180000, 0x0680000, 0x0C00000 },
    { 0x17, 0x0580000, 0x0200000, 0x0E00000, 0x0C80000 },
    { 0x17, 0x0300000, 0x0300000, 0x0D00000, 0x1800000 },
    { 0x18, 0x0B00000, 0x0400000, 0x1C00000, 0x1900000 },
    { 0x18, 0x0600000, 0x0600000, 0x1A00000, 0x3000000 },
    { 0x19, 0x1600000, 0x0800000, 0x3800000, 0x3200000 },
    { 0x19, 0x0C00000, 0x0C00000, 0x3400000, 0x6000000 },
    { 0x1A, 0x2C00000, 0x1000000, 0x7000000, 0x6400000 },
    { 0x1A, 0x1800000, 0x1800000, 0x6800000, 0xC000000 },
}};

} // namespace

int Decoder::get_unary(BitStreamReader& gb, int stop, int len) {
    int i;
    for (i = 0; i < len && gb.get_bits1() != stop; i++)
        ;
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
        if (x >= code.escape && gb.get_bits1()) {
            x |= 1 << code.init;
            if (x >= code.aescape) {
                int32_t scale = get_unary(gb, 1, 9);
                if (scale == 9) {
                    int scale_bits = gb.get_bits(3);
                    if (scale_bits > 0) {
                        if (scale_bits == 7) {
                            int old_scale_bits = scale_bits;
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
                    x += code.scale * scale - code.escape;
                }
            } else {
                x -= code.escape;
            }
        }
        decoded[i] = (x >> 1) ^ -(x & 1);
    }
}

void Decoder::decode_lpc(int32_t* coeffs, int mode, int length) {
    if (length < 2) return;

    if (mode == 1) {
        uint32_t a1 = *coeffs++;
        for (int i = 0; i < (length - 1) >> 1; i++) {
            *coeffs += a1;
            coeffs[1] += static_cast<uint32_t>(*coeffs);
            a1 = coeffs[1];
            coeffs += 2;
        }
        if ((length - 1) & 1) {
            *coeffs += a1;
        }
    } else if (mode == 2) {
        uint32_t a1 = coeffs[1];
        uint32_t a2 = a1 + *coeffs;
        coeffs[1] = a2;
        if (length > 2) {
            coeffs += 2;
            for (int i = 0; i < (length - 2) >> 1; i++) {
                uint32_t a3 = *coeffs + a1;
                uint32_t a4 = a3 + a2;
                *coeffs = a4;
                a1 = coeffs[1] + a3;
                a2 = a1 + a4;
                coeffs[1] = a2;
                coeffs += 2;
            }
            if (length & 1) {
                *coeffs += a1 + a2;
            }
        }
    } else if (mode == 3) {
        uint32_t a1 = coeffs[1];
        uint32_t a2 = a1 + *coeffs;
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

    if (gb.get_bits1()) {
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
            int c = get_unary(gb, 1, 6);

            switch (c) {
            case 6:
                mode = gb.get_bits(6);
                break;
            case 5:
            case 4:
            case 3: {
                int sign = gb.get_bits1();
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
                if (i == wlength) break;
            } while (coding_mode_[i] == mode);

            decode_segment(mode, decoded, len, gb);
            decoded += len;
        }
    } else {
        int mode = gb.get_bits(6);
        decode_segment(mode, decoded, length, gb);
    }
}

} // namespace takdecomp
