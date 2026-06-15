#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t c = 0xCE04B7U;
    printf("Initial checksum: %08X\n", c);
    const AVCRC *ctx = av_crc_get_table(AV_CRC_24_IEEE);
    printf("ctx[256] = %d\n", ctx[256]);
    c = av_crc(ctx, c, data, 4);
    printf("Result: %08X\n", c);
    return 0;
}
