#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    const AVCRC *table = av_crc_get_table(AV_CRC_24_IEEE);
    printf("Table[1] = %X\n", table[1]);
    return 0;
}
