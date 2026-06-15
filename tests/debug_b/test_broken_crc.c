#include <stdio.h>
#include <stdint.h>

uint32_t table[256];
void init() {
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x80000000) ? (poly << 8) : 0);
        // FFMPEG av_crc_init without le puts the 24-bit polynomial shifted left by 8!
        // Wait, FFMPEG hardcoded table has FB4C86!
        // But FB4C86 is av_bswap32(0x00864CFB).
        // Let's just use the known FB4C86
    }
}
uint32_t broken_ff_crc(const uint32_t* ctx, uint32_t crc, const uint8_t *buffer, size_t length) {
    const uint8_t *end = buffer + length;
    while (buffer < end)
        crc = ctx[((crc >> 16) ^ *buffer++) & 0xFF] ^ (crc << 8);
    return crc & 0xFFFFFF;
}
int main() {
    // Just include ffmpeg headers to get the table
    return 0;
}
