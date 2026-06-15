#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    const AVCRC *t = av_crc_get_table(AV_CRC_24_IEEE);
    printf("Table[1]: %X\n", t[1]);
    return 0;
}
