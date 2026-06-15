#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 4, SEEK_SET); // Skip tBaK
    while (1) {
        uint8_t t;
        if (fread(&t, 1, 1, f) != 1) break;
        uint8_t type = t & 0x7f;
        uint8_t s[3];
        fread(s, 1, 3, f);
        uint32_t size = s[0] | (s[1] << 8) | (s[2] << 16);
        printf("Type: %d, Size: %d\n", type, size);
        
        if (type == 1 || type == 7 || type == 4) {
            if (size <= 3) {
                printf("ERROR: size <= 3\n");
                return 1;
            }
            uint8_t *buf = malloc(size - 3);
            fread(buf, 1, size - 3, f);
            uint8_t crc[3];
            fread(crc, 1, 3, f);
            uint32_t read_crc = (crc[0] << 16) | (crc[1] << 8) | crc[2];
            uint32_t calc_crc = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, buf, size - 3);
            if (read_crc != calc_crc) {
                printf("ERROR: CRC mismatch! read: %06X, calc: %06X\n", read_crc, calc_crc);
                return 1;
            }
            if (type == 7 && size != 11) {
                printf("ERROR: LAST_FRAME size != 11\n");
                return 1;
            }
        } else if (type == 0) {
            printf("END!\n");
            break;
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }
    printf("SUCCESS!\n");
    return 0;
}
