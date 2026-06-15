#include <stdio.h>
#include <stdint.h>

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 4, SEEK_SET); // Skip tBaK
    uint8_t t;
    fread(&t, 1, 1, f);
    uint8_t s[3];
    fread(s, 1, 3, f);
    uint32_t size = s[0] | (s[1] << 8) | (s[2] << 16);
    uint8_t payload[256];
    fread(payload, 1, size, f);
    printf("STREAMINFO (%s): ", argv[1]);
    for (int i = 0; i < size; i++) printf("%02X ", payload[i]);
    printf("\n");
    fclose(f);
    return 0;
}
