#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t crc = 0xCE04B7U;
    const AVCRC *ctx = av_crc_get_table(AV_CRC_24_IEEE);
    
    uint32_t buf_val = *(const uint32_t *) data;
    crc ^= buf_val;
    
    printf("crc after XOR = %08X\n", crc);
    printf("crc >> 24 = %02X\n", crc >> 24);
    
    uint32_t c0 = ctx[0 * 256 + ((crc >> 24) & 0xFF)];
    uint32_t c1 = ctx[1 * 256 + ((crc >> 16) & 0xFF)];
    uint32_t c2 = ctx[2 * 256 + ((crc >> 8 ) & 0xFF)];
    uint32_t c3 = ctx[3 * 256 + ( crc        & 0xFF)];
    
    printf("c0 = %08X, c1 = %08X, c2 = %08X, c3 = %08X\n", c0, c1, c2, c3);
    
    crc = c3 ^ c2 ^ c1 ^ c0;
    
    printf("Final CRC: %06X\n", crc & 0xFFFFFF);
    return 0;
}
