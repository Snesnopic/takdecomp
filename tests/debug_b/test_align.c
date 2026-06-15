#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>
#include <stdlib.h>
#include <string.h>

int main() {
    uint8_t *base = aligned_alloc(32, 32);
    for(int offset=0; offset<4; offset++) {
        uint8_t *data = base + offset;
        data[0] = 0xFF; data[1] = 0xA0; data[2] = 0x00; data[3] = 0x00;
        uint32_t c = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, data, 4);
        printf("Offset %d: %06X\n", offset, c & 0xFFFFFF);
    }
    return 0;
}
