#include "tak_encoder/encoder.hpp"
#include <stdexcept>

namespace takenc {

const CParam xcodes[50] = {
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
    { 0x0B, 0x0000300, 0x0000300, 0x0000D00, 0x0000180 },
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
};

static void write_unary(BitStreamWriter& bw, int val, int stop) {
    for (int i = 0; i < val; i++) {
        bw.write_bit(stop == 1 ? 0 : 1);
    }
    if (val < 9) {
        bw.write_bit(stop);
    }
}

void Encoder::encode_segment(int mode, const int32_t* data, int len, BitStreamWriter& bw) {
    if (mode == 0) return;
    if (mode < 0 || mode > 50) throw std::runtime_error("Invalid encode_segment mode");
    
    const auto& code = xcodes[mode - 1];
    
    for (int i = 0; i < len; i++) {
        int32_t v = data[i];
        uint32_t x = (static_cast<uint32_t>(v) << 1) ^ (v >> 31);
        
        if (x < code.escape) {
            bw.write_bits(x, code.init);
            continue;
        }
        if (x < (1U << code.init)) {
            bw.write_bits(x, code.init);
            bw.write_bit(0);
            continue;
        }
        
        uint32_t v_base_test = x + code.escape;
        if (v_base_test < code.aescape) {
            bw.write_bits(v_base_test & ((1U << code.init) - 1), code.init);
            bw.write_bit(1);
            continue;
        }
        
        uint32_t val;
        if (x >= code.aescape + code.bias) {
            val = x - code.bias - code.aescape;
            uint32_t scale = val / code.scale;
            uint32_t v_base = code.aescape + (val % code.scale);
            
            bw.write_bits(v_base & ((1U << code.init) - 1), code.init);
            bw.write_bit(1);
            write_unary(bw, 9, 1);
            
            if (scale > 0) {
                int scale_bits = 0;
                uint32_t s = scale - 1;
                while ((1U << scale_bits) <= s) scale_bits++;
                if (scale_bits == 0) scale_bits = 1;
                
                if (scale_bits >= 7) {
                    bw.write_bits(7, 3);
                    bw.write_bits(scale_bits - 7, 5);
                } else {
                    bw.write_bits(scale_bits, 3);
                }
                bw.write_bits(scale - 1, scale_bits);
            } else {
                bw.write_bits(0, 3);
            }
        } else {
            val = x + code.escape - code.aescape;
            uint32_t scale = val / code.scale;
            uint32_t v_base = code.aescape + (val % code.scale);
            
            bw.write_bits(v_base & ((1U << code.init) - 1), code.init);
            bw.write_bit(1);
            write_unary(bw, scale, 1);
        }
    }
}

int Encoder::calc_bits_needed(int mode, const int32_t* data, int len) {
    if (mode == 0) return 0;
    const auto& code = xcodes[mode - 1];
    int bits = 0;
    
    for (int i = 0; i < len; i++) {
        int32_t v = data[i];
        uint32_t x = (static_cast<uint32_t>(v) << 1) ^ (v >> 31);
        
        if (x < code.escape) {
            bits += code.init;
            continue;
        }
        if (x < (1U << code.init)) {
            bits += code.init + 1;
            continue;
        }
        
        uint32_t v_base_test = x + code.escape;
        if (v_base_test < code.aescape) {
            bits += code.init + 1;
            continue;
        }
        
        uint32_t val;
        if (x >= code.aescape + code.bias) {
            val = x - code.bias - code.aescape;
            uint32_t scale = val / code.scale;
            
            bits += code.init + 1; // v_base + flag
            bits += 9; // unary 9 zeros
            
            if (scale > 0) {
                int scale_bits = 0;
                uint32_t s = scale - 1;
                while ((1U << scale_bits) <= s) scale_bits++;
                if (scale_bits == 0) scale_bits = 1;
                
                if (scale_bits >= 7) {
                    bits += 3 + 5;
                } else {
                    bits += 3;
                }
                bits += scale_bits;
            } else {
                bits += 3;
            }
        } else {
            val = x + code.escape - code.aescape;
            uint32_t scale = val / code.scale;
            bits += code.init + 1; // v_base + flag
            bits += scale + 1; // unary zeros + 1 stop bit
        }
    }
    return bits;
}

} // namespace takenc
