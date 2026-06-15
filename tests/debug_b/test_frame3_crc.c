#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0xff, 0xa0, 0x19, 0x00, 0x00, 0x10, 0x2b};
    uint32_t c = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, data, 7);
    printf("FFMPEG CRC 7 bytes: %06X\n", c);
    return 0;
}
