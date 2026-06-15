#include <stdio.h>
#include <stdint.h>

uint32_t table[256];
void init() {
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 16;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x800000) ? 0x864CFB : 0);
        table[i] = c & 0xFFFFFF;
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
    printf("MY CRC: %X\n", crc24(data, 4));
    return 0;
}
