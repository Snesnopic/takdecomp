#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    uint32_t table[256];
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x80000000) ? (poly << 8) : 0);
        table[i] = ((c << 24) & 0xFF000000) | ((c << 8) & 0x00FF0000) | ((c >> 8) & 0x0000FF00) | ((c >> 24) & 0x000000FF);
    }

    const AVCRC *orig = av_crc_get_table(AV_CRC_24_IEEE);
    int diff = 0;
    for(int i=0; i<256; i++) {
        if (table[i] != orig[i]) {
            printf("Diff at %d: my %08X, orig %08X\n", i, table[i], orig[i]);
            diff = 1;
            break;
        }
    }
    if (!diff) printf("Tables are IDENTICAL!\n");
    return 0;
}
