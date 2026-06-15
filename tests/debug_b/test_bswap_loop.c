#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

static inline uint32_t bswap32(uint32_t x) {
    return ((x << 24) & 0xFF000000) |
           ((x <<  8) & 0x00FF0000) |
           ((x >>  8) & 0x0000FF00) |
           ((x >> 24) & 0x000000FF);
}

int main() {
    const AVCRC *ctx = av_crc_get_table(AV_CRC_24_IEEE);
    uint32_t crc = bswap32(0xCE04B7);
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    for (int i=0; i<4; i++) {
        crc = ctx[((uint8_t) crc) ^ data[i]] ^ (crc >> 8);
    }
    printf("Result: %X\n", bswap32(crc));
    printf("Masked Result: %X\n", bswap32(crc) >> 8);
    return 0;
}
