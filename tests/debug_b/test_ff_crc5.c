#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t c = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, data, 4);
    printf("FFMPEG CRC: %X, and masked: %X\n", c, c & 0xFFFFFF);
    return 0;
}
