#include <stdio.h>
#include <stdint.h>

uint32_t table[256];
void init() {
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x80000000) ? (poly << 8) : 0);
        table[i] = c >> 8;
    }
}

uint32_t crc24(const uint8_t *data, int len) {
    uint32_t crc = 0xCE04B7;
    for (int i = 0; i < len; i++) {
        crc = table[((crc >> 16) ^ data[i]) & 0xFF] ^ (crc << 8);
        crc &= 0xFFFFFF;
    }
    return crc;
}

int main() {
    init();
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    printf("MY CRC 2: %X\n", crc24(data, 4));
    return 0;
}
