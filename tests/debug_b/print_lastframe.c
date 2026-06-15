#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 4, SEEK_SET); // Skip tBaK
    while (1) {
        uint8_t t;
        if (fread(&t, 1, 1, f) != 1) break;
        uint8_t s[3];
        fread(s, 1, 3, f);
        uint32_t size = s[0] | (s[1] << 8) | (s[2] << 16);
        if ((t & 0x7f) == 7) {
            uint8_t payload[256];
            fread(payload, 1, size, f);
            printf("LAST_FRAME: ");
            for (int i = 0; i < size; i++) printf("%02X ", payload[i]);
            printf("\n");
            break;
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }
    fclose(f);
    return 0;
}
