#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t poly = 0x864CFB;
    for (int i = 0; i < 2; i++) {
        uint32_t c = i << 24;
        for (int j = 0; j < 8; j++)
            c = (c << 1) ^ ((c & 0x80000000) ? (poly << 8) : 0);
        uint32_t t = ((c << 24) & 0xFF000000) | ((c << 8) & 0x00FF0000) | ((c >> 8) & 0x0000FF00) | ((c >> 24) & 0x000000FF);
        printf("table[%d] = %X\n", i, t);
    }
    return 0;
}
