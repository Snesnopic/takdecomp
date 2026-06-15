#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t ctx[1024] = {0};
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x80000000) ? (poly << 8) : 0);
        ctx[i] = ((c << 24) & 0xFF000000) | ((c << 8) & 0x00FF0000) | ((c >> 8) & 0x0000FF00) | ((c >> 24) & 0x000000FF);
    }
    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 3; j++)
            ctx[256 * (j + 2) + i] =
                (ctx[256 * (j + 1) + i] >> 8) ^
                ctx[ctx[256 * (j + 1) + i] & 0xFF];

    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t crc = 0xCE04B7U;
    
    const uint8_t *buffer = data;
    const uint8_t *end = data + 4;
    
    while (((intptr_t) buffer & 3) && buffer < end)
        crc = ctx[((uint8_t) crc) ^ *buffer++] ^ (crc >> 8);

    while (buffer < end - 3) {
        crc ^= *(const uint32_t *) buffer; // Assuming LE machine
        buffer += 4;
        crc = ctx[3 * 256 + ( crc        & 0xFF)] ^
              ctx[2 * 256 + ((crc >> 8 ) & 0xFF)] ^
              ctx[1 * 256 + ((crc >> 16) & 0xFF)] ^
              ctx[0 * 256 + ((crc >> 24)       )];
    }

    while (buffer < end)
        crc = ctx[((uint8_t) crc) ^ *buffer++] ^ (crc >> 8);

    printf("1024 CRC: %06X\n", crc & 0xFFFFFF);
    return 0;
}
