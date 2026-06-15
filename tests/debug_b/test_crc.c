#include <stdio.h>
#include <stdint.h>

uint32_t crc24_table[256];

void init_crc() {
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (poly ^ (c >> 1)) : (c >> 1);
        }
        crc24_table[i] = c;
    }
}

uint32_t compute_crc24(const uint8_t *data, size_t len) {
    uint32_t crc = 0xCE04B7;
    for (size_t i = 0; i < len; i++) {
        crc = crc24_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc & 0xFFFFFF;
}

int main() {
    init_crc();
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t c = compute_crc24(data, 4);
    printf("CRC: %X\n", c);
    return 0;
}
