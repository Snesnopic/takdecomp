#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 4, SEEK_SET); // Skip tBaK
    while (1) {
        uint8_t t;
        if (fread(&t, 1, 1, f) != 1) break;
        uint8_t type = t & 0x7f;
        int is_last = (t & 0x80) ? 1 : 0;
        uint8_t s[3];
        fread(s, 1, 3, f);
        uint32_t size = s[0] | (s[1] << 8) | (s[2] << 16);
        printf("Type: %d, is_last: %d, Size: %d\n", type, is_last, size);
        if (type == 0) break;
        fseek(f, size, SEEK_CUR);
    }
    return 0;
}
