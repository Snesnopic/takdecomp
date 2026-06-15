#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 4, SEEK_SET); // Skip tBaK
    while (1) {
        uint8_t t;
        if (fread(&t, 1, 1, f) != 1) break;
        uint8_t type = t & 0x7f;
        uint8_t s[3];
        fread(s, 1, 3, f);
        uint32_t size = s[0] | (s[1] << 8) | (s[2] << 16);
        if (type == 3) {
            uint8_t payload[2048];
            fread(payload, 1, size, f);
            printf("Type 3: ");
            for (int i = 0; i < size; i++) {
                if (payload[i] >= 32 && payload[i] <= 126) printf("%c", payload[i]);
                else printf("\\x%02X", payload[i]);
            }
            printf("\n");
            break;
        } else if (type == 0) {
            break;
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }
    fclose(f);
    return 0;
}
