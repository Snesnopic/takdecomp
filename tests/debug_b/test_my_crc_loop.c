#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t table[256];
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x80000000) ? (poly << 8) : 0);
        // FFMPEG av_bswap32(c):
        table[i] = ((c << 24) & 0xFF000000) | ((c << 8) & 0x00FF0000) | ((c >> 8) & 0x0000FF00) | ((c >> 24) & 0x000000FF);
    }

    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t crc = 0xCE04B7U;
    
    // FFMPEG loop:
    for (int i=0; i<4; i++) {
        crc = table[((crc >> 16) ^ data[i]) & 0xFF] ^ (crc << 8);
    }
    printf("Custom FFMPEG CRC loop: %08X\n", crc);
    return 0;
}
