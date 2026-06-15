#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    const AVCRC *table = av_crc_get_table(AV_CRC_24_IEEE);
    uint32_t crc = 0xCE04B7U;
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    for(int i=0; i<4; i++) {
        crc = table[((crc >> 16) ^ data[i]) & 0xFF] ^ (crc << 8);
        printf("i=%d, crc=%06X\n", i, crc);
    }
    printf("FFMPEG manual CRC: %06X\n", crc & 0xFFFFFF);
    return 0;
}
