#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t crc = 0xCE04B7U;
    const AVCRC *ctx = av_crc_get_table(AV_CRC_24_IEEE);
    
    // Simulate FFMPEG's weird loop
    const uint8_t *buffer = data;
    const uint8_t *end = data + 4;
    
    while (((intptr_t) buffer & 3) && buffer < end)
        crc = ctx[((uint8_t) crc) ^ *buffer++] ^ (crc >> 8);

    while (buffer < end - 3) {
        crc ^= *(const uint32_t *) buffer; // Assuming LE machine
        buffer += 4;
        crc = ctx[3 * 256 + ( crc        & 0xFF)] ^
              ctx[2 * 256 + ((crc >> 8 ) & 0xFF)] ^
              ctx[1 * 256 + ((crc >> 16) & 0xFF)] ^
              ctx[0 * 256 + ((crc >> 24)       )];
    }

    while (buffer < end)
        crc = ctx[((uint8_t) crc) ^ *buffer++] ^ (crc >> 8);

    printf("FFMPEG WEIRD CRC: %06X\n", crc & 0xFFFFFF);
    return 0;
}
