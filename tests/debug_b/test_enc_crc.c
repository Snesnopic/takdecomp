#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0x03, 0x03, 0x02, 0x00};
    uint32_t c = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, data, 4);
    printf("FFMPEG CRC 4 bytes: %06X\n", c);
    return 0;
}
