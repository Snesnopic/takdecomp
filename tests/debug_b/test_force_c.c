#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    AVCRC table[257];
    // Copy the precomputed table
    const AVCRC *orig = av_crc_get_table(AV_CRC_24_IEEE);
    for(int i=0; i<257; i++) table[i] = orig[i];
    
    // Set table[0] to 0 but it's already 0.
    // To bypass the aarch64 asm optimization, we must ensure ctx[0] is 0!
    // But table[0] is 0x000000! So av_crc WILL use the asm optimization!
    // Wait! In av_crc:
    // if (ctx[0]) { return ff_crc_aarch64(...); }
    // If ctx[0] is 0, it falls back to C loop!!!
    // And for AV_CRC_24_IEEE, ctx[0] is 0x000000!!!
    // SO it ALREADY falls back to the C loop!!!
    printf("ctx[0] = %08X\n", orig[0]);
    return 0;
}
