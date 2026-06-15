#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t table[256] = {0x000000, 0xFB4C86, 0x0DD58A, 0xF6990C, 0xE1E693, 0x1AAA15, 0xEC3319}; // enough for first element
    uint8_t data[] = {0xFF, 0xA0, 0x00, 0x00};
    uint32_t crc = 0xCE04B7;
    for (int i=0; i<4; i++) {
        // we can't run this without full table
    }
    return 0;
}
