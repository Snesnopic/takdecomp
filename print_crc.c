#include <stdio.h>
#include <stdint.h>
int main() {
    uint32_t poly = 0x864CFB;
    uint32_t table[256];
    for (int i = 0; i < 256; i++) {
        uint32_t c = i << 16;
        for (int j = 0; j < 8; j++) {
            if (c & 0x800000)
                c = ((c << 1) ^ poly) & 0xFFFFFF;
            else
                c = (c << 1) & 0xFFFFFF;
        }
        table[i] = c;
    }
    for (int i = 0; i < 256; i++) {
        printf("0x%06X, ", table[i]);
        if (i % 8 == 7) printf("\n");
    }
    return 0;
}
