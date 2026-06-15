#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0x02, 0x0c, 0x11, 0x2b, 0x00, 0x00, 0x40, 0x4d, 0x09, 0x0a};
    uint32_t c = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, data, 10);
    printf("FFMPEG CRC 10 bytes: %06X\n", c);
    return 0;
}
