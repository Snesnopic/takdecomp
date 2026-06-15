#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint32_t table[256];
    av_crc_init(table, 0, 24, 0x864CFB, sizeof(table));
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t c = av_crc(table, 0xCE04B7U, data, 4);
    printf("FFMPEG CRC LE=0: %X\n", c);
    
    av_crc_init(table, 1, 24, 0x864CFB, sizeof(table));
    c = av_crc(table, 0xCE04B7U, data, 4);
    printf("FFMPEG CRC LE=1: %X\n", c);
    return 0;
}
