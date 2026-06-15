#include <stdio.h>
#include <stdint.h>

int main() {
    FILE *f = fopen("tests/their_short.tak", "rb");
    uint8_t header[4];
    fread(header, 1, 4, f);
    while (1) {
        uint8_t t;
        if (fread(&t, 1, 1, f) != 1) break;
        uint8_t s[3];
        fread(s, 1, 3, f);
        uint32_t size = s[0] | (s[1] << 8) | (s[2] << 16);
        printf("Type: %02X (is_last: %d, type_id: %d), Size: %d\n", t, (t & 0x80) ? 1 : 0, t & 0x7f, size);
        if (t & 0x80) break;
        fseek(f, size, SEEK_CUR);
    }
    fclose(f);
    return 0;
}
