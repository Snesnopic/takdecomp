#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    FILE *f = fopen("tests/debug_b/my.tak", "rb");
    if (!f) return 1;
    
    uint8_t buf[256];
    int r = fread(buf, 1, 256, f);
    fclose(f);
    
    for(int i=0; i<r-7; i++) {
        if(buf[i] == 0xFF && buf[i+1] == 0xA0) {
            uint32_t crc = av_crc(av_crc_get_table(AV_CRC_24_IEEE), 0xCE04B7U, buf+i, 4);
            uint32_t exp = (buf[i+4] << 16) | (buf[i+5] << 8) | buf[i+6];
            printf("Found sync at %d. FFMPEG computed CRC: %06X, Header CRC: %06X\n", i, crc, exp);
            break;
        }
    }
    return 0;
}
